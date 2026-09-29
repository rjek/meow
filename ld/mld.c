/* mld: a static linker for MEOW ELF32 relocatable objects. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "melf.h"

struct osec {
	char *name;
	uint32_t type;
	uint32_t flags;
	uint32_t align;
	uint32_t addr;		/* where it runs */
	uint32_t load;		/* where a flat image stores it */
	uint8_t *data;
	uint32_t size;
	int order;		/* 0 code, 1 data, 2 bss */
	int index;		/* output ELF section index */
};

struct input {
	const char *path;
	struct melf *e;
	struct osec **map;	/* per input section, NULL if not placed */
	uint32_t *off;		/* offset within the output section */
};

struct gsym {
	char *name;
	struct input *def;	/* NULL while undefined */
	uint32_t value;		/* final address once laid out */
	unsigned type;
	struct osec *sec;	/* NULL for absolute */
};

/* A member of an ar archive, loaded only if it defines a symbol that is
 * still undefined once everything named on the command line is in. */
struct member {
	char *name;
	const uint8_t *data;
	size_t size;
	bool loaded;
};

struct archive {
	const char *path;
	struct member *members;
	unsigned nmembers;
};

/* A place a 32-bit address was written, for the relocation lists of
 * loadable images and displaced data. */
struct place {
	uint32_t addr;		/* where the word is */
	uint32_t value;		/* what was written */
	bool relocatable;	/* the target is in a placed section */
};

static struct place *places;
static unsigned nplaces;
static const char **symfiles;	/* -S: executables whose symbols resolve ours */
static unsigned nsymfiles;
static bool bss_backwards;	/* -B */
static bool pad_flat;		/* -p: a flat image padded to a word, for what follows it */

static struct input *inputs;
static unsigned ninputs;
static struct osec **osecs;
static unsigned nosecs;
static struct gsym *gsyms;
static struct archive *archives;
static unsigned narchives;
static unsigned ngsyms;
static int error_count;

static void usage(void)
{
	fprintf(stderr, "usage: mld [-o output] [-f elf|bin|cfx] [-b base] [-d data base]\n"
		"           [-M map] [-e entry] [-S executable] [-B] [-p]\n"
		"           [-R start,end,file] input.o...\n");
	exit(2);
}

static void *xalloc(size_t n)
{
	void *p = calloc(1, n == 0 ? 1 : n);

	if (p == NULL) {
		fputs("mld: out of memory\n", stderr);
		exit(2);
	}
	return p;
}

static void *xrealloc(void *p, size_t n)
{
	p = realloc(p, n);
	if (p == NULL) {
		fputs("mld: out of memory\n", stderr);
		exit(2);
	}
	return p;
}

static char *xstrdup(const char *s)
{
	char *p = xalloc(strlen(s) + 1);

	strcpy(p, s);
	return p;
}

static void error(const char *file, const char *fmt, ...)
{
	va_list ap;

	fputs("mld: ", stderr);
	if (file != NULL) {
		fprintf(stderr, "%s: ", file);
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	error_count++;
}

static bool has_suffix(const char *s, const char *suffix)
{
	size_t n = strlen(s);
	size_t m = strlen(suffix);

	return n >= m && strcmp(s + n - m, suffix) == 0;
}

static uint32_t align_up(uint32_t v, uint32_t a)
{
	return a > 1 ? (v + a - 1) & ~(a - 1) : v;
}

/* ---- sections --------------------------------------------------------- */

static struct osec *osec_lookup(const char *name, uint32_t type,
				uint32_t flags)
{
	struct osec *s;
	unsigned i;

	for (i = 0; i < nosecs; i++) {
		if (strcmp(osecs[i]->name, name) == 0) {
			return osecs[i];
		}
	}
	s = xalloc(sizeof *s);
	s->name = xstrdup(name);
	s->type = type;
	s->flags = flags;
	s->align = 1;
	if (type == MELF_SHT_NOBITS) {
		s->order = 2;
	} else if ((flags & MELF_SHF_WRITE) == 0) {
		s->order = 0;		/* code and read-only data stay with the ROM */
	} else {
		s->order = 1;
	}
	osecs = xrealloc(osecs, (nosecs + 1) * sizeof *osecs);
	osecs[nosecs++] = s;
	return s;
}

static void merge_input(struct input *in)
{
	struct melf *e = in->e;
	unsigned i;

	in->map = xalloc(e->nsections * sizeof *in->map);
	in->off = xalloc(e->nsections * sizeof *in->off);
	for (i = 1; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];
		struct osec *o;
		uint32_t start;

		if ((s->type != MELF_SHT_PROGBITS && s->type != MELF_SHT_NOBITS) ||
		    (s->flags & MELF_SHF_ALLOC) == 0) {
			continue;
		}
		o = osec_lookup(s->name, s->type, s->flags);
		if (o->type != s->type) {
			error(in->path, "section '%s' is %s but was %s before",
			      s->name, s->type == MELF_SHT_NOBITS ? "bss" : "data",
			      o->type == MELF_SHT_NOBITS ? "bss" : "data");
			continue;
		}
		start = align_up(o->size, s->align);
		if (s->align > o->align) {
			o->align = s->align;
		}
		in->map[i] = o;
		in->off[i] = start;
		if (o->type != MELF_SHT_NOBITS && start + s->size > 0) {
			o->data = xrealloc(o->data, start + s->size);
			memset(o->data + o->size, 0, start - o->size);
			memcpy(o->data + start, s->data, s->size);
		}
		o->size = start + s->size;
	}
}

/* Stable ordering: code, then data, then bss, each in first-seen order. */
/* Code runs where the image is loaded.  Data and BSS run at data_base if
 * one was given (a RAM address, for an image in ROM), else straight after
 * the code; the flat image always stores the data straight after the code
 * for start-up code to copy. */
static void layout(uint32_t base, bool have_data_base, uint32_t data_base)
{
	struct osec **sorted = xalloc(nosecs * sizeof *sorted);
	unsigned n = 0;
	unsigned i;
	int order;
	uint32_t addr = base;
	uint32_t load;

	for (order = 0; order < 3; order++) {
		for (i = 0; i < nosecs; i++) {
			if (osecs[i]->order == order) {
				sorted[n++] = osecs[i];
			}
		}
	}
	free(osecs);
	osecs = sorted;
	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		if (o->align < 4) {
			o->align = 4;
		}
		if (o->order == 0) {
			addr = align_up(addr, o->align);
			o->addr = o->load = addr;
			addr += o->size;
		}
	}
	load = addr;
	if (have_data_base == true) {
		addr = data_base;
	}
	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		if (o->order != 0) {
			addr = align_up(addr, o->align);
			load = align_up(load, o->align);
			o->addr = addr;
			o->load = o->order == 1 ? load : addr;
			addr += o->size;
			if (o->order == 1) {
				load += o->size;
			}
		}
	}
}

/* ---- symbols ---------------------------------------------------------- */

static struct gsym *gsym_lookup(const char *name)
{
	struct gsym *g;
	unsigned i;

	for (i = 0; i < ngsyms; i++) {
		if (strcmp(gsyms[i].name, name) == 0) {
			return &gsyms[i];
		}
	}
	gsyms = xrealloc(gsyms, (ngsyms + 1) * sizeof *gsyms);
	g = &gsyms[ngsyms++];
	memset(g, 0, sizeof *g);
	g->name = xstrdup(name);
	return g;
}

/* Final address of a symbol defined in an input, or false if it lives in
 * a section that was not placed. */
static bool input_symbol_value(struct input *in, struct melf_symbol *s,
			       uint32_t *value, struct osec **sec)
{
	if (s->shndx == MELF_SHN_ABS) {
		*value = s->value;
		*sec = NULL;
		return true;
	}
	if (s->shndx <= 0 || (unsigned)s->shndx >= in->e->nsections ||
	    in->map[s->shndx] == NULL) {
		return false;
	}
	*sec = in->map[s->shndx];
	*value = (*sec)->addr + in->off[s->shndx] + s->value;
	return true;
}

static void collect_globals(void)
{
	unsigned k;

	for (k = 0; k < ninputs; k++) {
		struct input *in = &inputs[k];
		unsigned i;

		for (i = 1; i < in->e->nsymbols; i++) {
			struct melf_symbol *s = &in->e->symbols[i];
			struct gsym *g;

			if (s->bind != MELF_STB_GLOBAL) {
				continue;
			}
			g = gsym_lookup(s->name);
			if (s->shndx == MELF_SHN_UNDEF) {
				continue;
			}
			if (g->def != NULL) {
				error(in->path, "duplicate definition of '%s', "
				      "also defined in %s", s->name, g->def->path);
				continue;
			}
			if (input_symbol_value(in, s, &g->value, &g->sec) == false) {
				error(in->path, "symbol '%s' is in an unplaced section",
				      s->name);
				continue;
			}
			g->def = in;
			g->type = s->type;
		}
	}
}

static void define_absolute(const char *name, uint32_t value)
{
	struct gsym *g = gsym_lookup(name);

	if (g->def != NULL) {
		error(g->def->path, "'%s' is defined by the linker", name);
		return;
	}
	g->def = &inputs[0];
	g->value = value;
	g->type = MELF_STT_NOTYPE;
	g->sec = NULL;
}

/* __data_load is where the initialised data sits in the image, __data_start
 * where it must be when the program runs; they differ only in a flat image
 * linked with -d.  __bss_start to __bss_end must be zeroed. */
static void define_layout_symbols(bool flat)
{
	uint32_t data_load = 0, data_start = 0, data_end = 0;
	uint32_t bss_start = 0, bss_end = 0;
	bool have_data = false, have_bss = false;
	unsigned i;

	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		if (o->order == 1) {
			if (have_data == false) {
				data_load = flat == true ? o->load : o->addr;
				data_start = o->addr;
				have_data = true;
			}
			data_end = o->addr + o->size;
		} else if (o->order == 2) {
			if (have_bss == false) {
				bss_start = o->addr;
				have_bss = true;
			}
			bss_end = o->addr + o->size;
		}
	}
	if (have_data == false) {
		data_load = data_start = data_end = have_bss == true ? bss_start : 0;
	}
	if (have_bss == false) {
		bss_start = bss_end = data_end;
	}
	define_absolute("__data_load", data_load);
	define_absolute("__data_start", data_start);
	define_absolute("__data_end", data_end);
	define_absolute("__bss_start", bss_start);
	define_absolute("__bss_end", bss_end);
}

static void check_undefined(void)
{
	unsigned i;

	for (i = 0; i < ngsyms; i++) {
		struct gsym *g = &gsyms[i];
		char *list = NULL;
		size_t len = 0;
		unsigned k;

		if (g->def != NULL) {
			continue;
		}
		for (k = 0; k < ninputs; k++) {
			struct input *in = &inputs[k];
			unsigned j;

			for (j = 1; j < in->e->nsymbols; j++) {
				struct melf_symbol *s = &in->e->symbols[j];

				if (s->bind == MELF_STB_GLOBAL &&
				    s->shndx == MELF_SHN_UNDEF &&
				    strcmp(s->name, g->name) == 0) {
					size_t n = strlen(in->path);

					list = xrealloc(list, len + n + 3);
					sprintf(list + len, "%s%s", len > 0 ? ", " : "",
						in->path);
					len += n + (len > 0 ? 2 : 0);
					break;
				}
			}
		}
		error(NULL, "undefined symbol '%s' referenced in %s", g->name,
		      list != NULL ? list : "?");
		free(list);
	}
}

/* ---- relocations ------------------------------------------------------ */

static unsigned reloc_width(unsigned type)
{
	switch (type) {
	case R_MEOW_ABS8: return 1;
	case R_MEOW_ABS16: return 2;
	default: return 4;
	}
}

static void apply_relocs(struct input *in)
{
	struct melf *e = in->e;
	unsigned i;

	for (i = 1; i < e->nsections; i++) {
		struct osec *o = in->map[i];
		unsigned j;

		if (e->nrelas[i] == 0) {
			continue;
		}
		if (o == NULL) {
			error(in->path, "relocations against unplaced section '%s'",
			      e->sections[i].name);
			continue;
		}
		for (j = 0; j < e->nrelas[i]; j++) {
			struct melf_rela *r = &e->relas[i][j];
			struct melf_symbol *s;
			uint32_t place = o->addr + in->off[i] + r->offset;
			unsigned w = reloc_width(r->type);
			uint32_t v;
			struct osec *vsec;
			unsigned k;

			if (r->sym < 0 || (unsigned)r->sym >= e->nsymbols ||
			    r->offset + w > e->sections[i].size ||
			    o->type == MELF_SHT_NOBITS) {
				error(in->path, "bad relocation in section '%s'",
				      e->sections[i].name);
				continue;
			}
			s = &e->symbols[r->sym];
			if (s->bind == MELF_STB_GLOBAL) {
				struct gsym *g = gsym_lookup(s->name);

				if (g->def == NULL) {
					continue;	/* reported already */
				}
				v = g->value;
			} else if (input_symbol_value(in, s, &v, &vsec) == false) {
				error(in->path, "relocation against unplaced symbol '%s'",
				      s->name);
				continue;
			}
			v += (uint32_t)r->addend;
			switch (r->type) {
			case R_MEOW_ABS32:
			case R_MEOW_ABS16:
			case R_MEOW_ABS8:
				break;
			case R_MEOW_REL32:
				v -= place;
				break;
			default:
				error(in->path, "unknown relocation type %u in '%s'",
				      r->type, e->sections[i].name);
				continue;
			}
			if (w < 4 && (v >> (8 * w)) != 0 &&
			    (int32_t)v >> (8 * w) != -1) {
				error(in->path, "value 0x%x at 0x%08x does not fit in "
				      "%u bytes", v, place, w);
				continue;
			}
			for (k = 0; k < w; k++) {
				o->data[in->off[i] + r->offset + k] = (uint8_t)(v >> (8 * k));
			}
			if (r->type == R_MEOW_ABS32) {
				places = xrealloc(places, (nplaces + 1) * sizeof *places);
				places[nplaces].addr = place;
				places[nplaces].value = v;
				places[nplaces].relocatable = s->bind == MELF_STB_GLOBAL ?
					gsym_lookup(s->name)->sec != NULL : vsec != NULL;
				nplaces++;
			}
		}
	}
}

/* -S: an executable's global symbols stand in for whatever is still
 * undefined, as absolute addresses.  A program links against the kernel
 * and the shared library in ROM this way. */
static void resolve_from_symfiles(void)
{
	unsigned n;

	for (n = 0; n < nsymfiles; n++) {
		char err[128];
		struct melf *e = melf_read(symfiles[n], err, sizeof err);
		unsigned i;

		if (e == NULL) {
			error(symfiles[n], "%s", err);
			continue;
		}
		if (e->e_type != MELF_ET_EXEC) {
			error(symfiles[n], "not an executable");
			continue;
		}
		for (i = 1; i < e->nsymbols; i++) {
			struct melf_symbol *sym = &e->symbols[i];
			unsigned k;

			if (sym->bind != MELF_STB_GLOBAL || sym->shndx == MELF_SHN_UNDEF) {
				continue;
			}
			for (k = 0; k < ngsyms; k++) {
				if (gsyms[k].def == NULL && strcmp(gsyms[k].name, sym->name) == 0) {
					define_absolute(sym->name, sym->value);
					break;
				}
			}
		}
	}
}

/* -B: lay the bss sections out in reverse input order, so that the last
 * inputs' bss follows their data with nothing between.  The shared
 * library's data and bss become one range to copy per process. */
static void reverse_bss(void)
{
	unsigned i;

	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];
		uint32_t size = 0;
		unsigned k;

		if (o->type != MELF_SHT_NOBITS) {
			continue;
		}
		for (k = ninputs; k-- > 0;) {
			struct input *in = &inputs[k];
			unsigned j;

			for (j = 1; j < in->e->nsections; j++) {
				struct melf_section *sec = &in->e->sections[j];

				if (in->map[j] != o) {
					continue;
				}
				size = align_up(size, sec->align > 0 ? sec->align : 1);
				in->off[j] = size;
				size += sec->size;
			}
		}
		o->size = size;
	}
}

static uint32_t symbol_value(const char *name)
{
	struct gsym *g = gsym_lookup(name);

	if (g->def == NULL) {
		error(NULL, "-R: '%s' is not defined", name);
		return 0;
	}
	return g->value;
}

/* -R: the words within [start, end) that point within [start, end), as
 * a table for whoever copies that range elsewhere and must move them. */
static void write_reloc_list(const char *spec)
{
	char *copy = xstrdup(spec);
	char *a = strtok(copy, ",");
	char *b = a != NULL ? strtok(NULL, ",") : NULL;
	char *path = b != NULL ? strtok(NULL, ",") : NULL;
	uint32_t start, end, count = 0;
	unsigned i;
	FILE *f;

	if (path == NULL) {
		usage();
	}
	start = symbol_value(a);
	end = symbol_value(b);
	f = fopen(path, "wb");
	if (f == NULL) {
		error(NULL, "cannot write '%s'", path);
		return;
	}
	for (i = 0; i < nplaces; i++) {
		if (places[i].addr >= start && places[i].addr < end &&
		    places[i].value >= start && places[i].value < end) {
			count++;
		}
	}
	fwrite("CFRL", 1, 4, f);
	fwrite(&count, 4, 1, f);
	for (i = 0; i < nplaces; i++) {
		if (places[i].addr >= start && places[i].addr < end &&
		    places[i].value >= start && places[i].value < end) {
			fwrite(&places[i].addr, 4, 1, f);
		}
	}
	if (fclose(f) != 0) {
		error(NULL, "write error on '%s'", path);
	}
	free(copy);
}

/* ---- output ----------------------------------------------------------- */

static void write_flat(const char *path, uint32_t base)
{
	FILE *f = fopen(path, "wb");
	uint32_t pos = base;
	unsigned i;

	if (f == NULL) {
		error(NULL, "cannot write '%s'", path);
		return;
	}
	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		if (o->type == MELF_SHT_NOBITS || o->size == 0) {
			continue;
		}
		if (o->load < pos) {
			error(NULL, "section '%s' overlaps the one before it", o->name);
			break;
		}
		while (pos < o->load) {
			fputc(0, f);
			pos++;
		}
		fwrite(o->data, 1, o->size, f);
		pos += o->size;
	}
	while (pad_flat == true && pos % 4 != 0) {
		fputc(0, f);
		pos++;
	}
	if (fclose(f) != 0) {
		error(NULL, "write error on '%s'", path);
	}
}

/* A loadable image: linked at 0 with data following code, a header, the
 * bytes, then the offsets of every word that holds an address and must
 * have the load address added.  Words that name absolute symbols (the
 * kernel, the library) are left alone. */
static void write_cfx(const char *path, uint32_t entry)
{
	FILE *f = fopen(path, "wb");
	uint32_t image_size = 0, mem_size = 0, pos = 0, n = 0, hdr[6];
	unsigned i;

	if (f == NULL) {
		error(NULL, "cannot write '%s'", path);
		return;
	}
	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		if (o->addr + o->size > mem_size) {
			mem_size = o->addr + o->size;
		}
		if (o->type != MELF_SHT_NOBITS && o->load + o->size > image_size) {
			image_size = o->load + o->size;
		}
	}
	image_size = align_up(image_size, 4);
	mem_size = align_up(mem_size, 4);
	for (i = 0; i < nplaces; i++) {
		n += places[i].relocatable;
	}
	memcpy(hdr, "CFX1", 4);
	hdr[1] = image_size;
	hdr[2] = mem_size;
	hdr[3] = entry;
	hdr[4] = n;
	hdr[5] = 0;
	fwrite(hdr, 4, 6, f);
	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		if (o->type == MELF_SHT_NOBITS || o->size == 0) {
			continue;
		}
		while (pos < o->load) {
			fputc(0, f);
			pos++;
		}
		fwrite(o->data, 1, o->size, f);
		pos += o->size;
	}
	while (pos < image_size) {
		fputc(0, f);
		pos++;
	}
	for (i = 0; i < nplaces; i++) {
		if (places[i].relocatable) {
			fwrite(&places[i].addr, 4, 1, f);
		}
	}
	if (fclose(f) != 0) {
		error(NULL, "write error on '%s'", path);
	}
}

static void write_elf(const char *path, uint32_t entry)
{
	struct melf *e = melf_new(MELF_ET_EXEC);
	unsigned i;
	unsigned k;

	e->entry = entry;
	for (i = 0; i < nosecs; i++) {
		struct osec *o = osecs[i];

		o->index = melf_add_section(e, o->name, o->type, o->flags,
					    o->data, o->size, o->align);
		e->sections[o->index].addr = o->addr;
	}
	/* locals first, then globals, as ELF requires */
	for (k = 0; k < ninputs; k++) {
		struct input *in = &inputs[k];

		for (i = 1; i < in->e->nsymbols; i++) {
			struct melf_symbol *s = &in->e->symbols[i];
			uint32_t v;
			struct osec *sec;

			if (s->bind != MELF_STB_LOCAL || s->type == MELF_STT_SECTION ||
			    s->name[0] == '\0' ||
			    input_symbol_value(in, s, &v, &sec) == false) {
				continue;
			}
			melf_add_symbol(e, s->name, v, MELF_STB_LOCAL, s->type,
					sec != NULL ? sec->index : MELF_SHN_ABS);
		}
	}
	for (i = 0; i < ngsyms; i++) {
		struct gsym *g = &gsyms[i];

		melf_add_symbol(e, g->name, g->value, MELF_STB_GLOBAL, g->type,
				g->sec != NULL ? g->sec->index : MELF_SHN_ABS);
	}
	if (melf_write(e, path) == false) {
		error(NULL, "cannot write '%s'", path);
	}
	melf_free(e);
}

static void write_map(FILE *f)
{
	unsigned i;
	unsigned k;

	fputs("Sections\n", f);
	for (i = 0; i < nosecs; i++) {
		fprintf(f, "  %08x %8u %s", osecs[i]->addr, osecs[i]->size,
			osecs[i]->name);
		if (osecs[i]->load != osecs[i]->addr) {
			fprintf(f, " (stored at %08x)", osecs[i]->load);
		}
		fputc('\n', f);
	}
	fputs("Symbols\n", f);
	for (i = 0; i < ngsyms; i++) {
		fprintf(f, "  %08x %s\n", gsyms[i].value, gsyms[i].name);
	}
	for (k = 0; k < ninputs; k++) {
		struct input *in = &inputs[k];

		for (i = 1; i < in->e->nsymbols; i++) {
			struct melf_symbol *s = &in->e->symbols[i];
			uint32_t v;
			struct osec *sec;

			if (s->bind != MELF_STB_LOCAL || s->type == MELF_STT_SECTION ||
			    s->name[0] == '\0' ||
			    input_symbol_value(in, s, &v, &sec) == false) {
				continue;
			}
			fprintf(f, "  %08x %s (local in %s)\n", v, s->name, in->path);
		}
	}
}

/* The assembler stores a relocatable file's entry as an offset within the
 * entry symbol's section, so look for a code symbol at that offset. */
/* Entry point: -e, else the assembler's __entry symbol, else start, else 0. */
static uint32_t find_entry(const char *name)
{
	static const char *const defaults[] = { "__entry", "start", "main" };
	unsigned i;
	unsigned d;

	if (name != NULL) {
		for (i = 0; i < ngsyms; i++) {
			if (strcmp(gsyms[i].name, name) == 0 && gsyms[i].def != NULL) {
				return gsyms[i].value;
			}
		}
		error(NULL, "entry symbol '%s' is not defined", name);
		return 0;
	}
	for (d = 0; d < sizeof defaults / sizeof defaults[0]; d++) {
		for (i = 0; i < ngsyms; i++) {
			if (strcmp(gsyms[i].name, defaults[d]) == 0 &&
			    gsyms[i].def != NULL) {
				return gsyms[i].value;
			}
		}
	}
	return 0;
}

/* ---- archives ---------------------------------------------------------- */

static bool is_archive(const char *path)
{
	FILE *f = fopen(path, "rb");
	char magic[8];
	bool yes;

	if (f == NULL) {
		return false;
	}
	yes = fread(magic, 1, 8, f) == 8 && memcmp(magic, "!<arch>\n", 8) == 0;
	fclose(f);
	return yes;
}

static uint8_t *read_whole(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	long n;
	uint8_t *p;

	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0) {
		fclose(f);
		return NULL;
	}
	p = xalloc((size_t)n + 1);
	if (fread(p, 1, (size_t)n, f) != (size_t)n) {
		fclose(f);
		free(p);
		return NULL;
	}
	fclose(f);
	*len = (size_t)n;
	return p;
}

/* The common ar format as GNU ar writes it: 60-byte headers, names ending
 * in '/', a "/" symbol table (ignored: members are examined directly) and
 * a "//" table of names too long for the header, referred to as "/offset". */
static void read_archive(const char *path)
{
	size_t len;
	uint8_t *img = read_whole(path, &len);
	struct archive *a;
	const char *longnames = NULL;
	size_t longnames_len = 0;
	size_t pos = 8;

	if (img == NULL) {
		error(path, "cannot read");
		return;
	}
	archives = xrealloc(archives, (narchives + 1) * sizeof *archives);
	a = &archives[narchives++];
	memset(a, 0, sizeof *a);
	a->path = path;
	while (pos + 60 <= len) {
		const char *h = (const char *)img + pos;
		size_t size = (size_t)strtoul(h + 48, NULL, 10);
		const uint8_t *data = img + pos + 60;
		char name[17];
		struct member *m;

		if (memcmp(h + 58, "`\n", 2) != 0 || pos + 60 + size > len) {
			error(path, "bad archive member header");
			break;
		}
		memcpy(name, h, 16);
		name[16] = '\0';
		pos += 60 + size + (size & 1);
		if (strncmp(name, "/ ", 2) == 0) {
			continue;			/* the symbol table */
		}
		if (strncmp(name, "// ", 3) == 0) {
			longnames = (const char *)data;
			longnames_len = size;
			continue;
		}
		a->members = xrealloc(a->members, (a->nmembers + 1) * sizeof *a->members);
		m = &a->members[a->nmembers++];
		memset(m, 0, sizeof *m);
		if (name[0] == '/' && longnames != NULL) {
			size_t off = (size_t)strtoul(name + 1, NULL, 10);
			size_t n = 0;

			while (off + n < longnames_len && longnames[off + n] != '/' &&
			       longnames[off + n] != '\n') {
				n++;
			}
			m->name = xalloc(n + 1);
			memcpy(m->name, longnames + off, n);
			m->name[n] = '\0';
		} else {
			char *slash = strchr(name, '/');

			if (slash != NULL) {
				*slash = '\0';
			}
			m->name = xstrdup(name);
		}
		m->data = data;
		m->size = size;
	}
}

/* Names of every global symbol referenced by an input and defined by none. */
static char **undefined_names(unsigned *count)
{
	char **names = NULL;
	unsigned n = 0;
	unsigned k;
	unsigned i;

	for (k = 0; k < ninputs; k++) {
		struct melf *e = inputs[k].e;

		if (e == NULL) {
			continue;
		}
		for (i = 1; i < e->nsymbols; i++) {
			struct melf_symbol *sym = &e->symbols[i];

			if (sym->bind == MELF_STB_GLOBAL && sym->shndx == MELF_SHN_UNDEF) {
				names = xrealloc(names, (n + 1) * sizeof *names);
				names[n++] = sym->name;
			}
		}
	}
	/* strike the ones some input defines */
	for (k = 0; k < ninputs; k++) {
		struct melf *e = inputs[k].e;

		if (e == NULL) {
			continue;
		}
		for (i = 1; i < e->nsymbols; i++) {
			struct melf_symbol *sym = &e->symbols[i];
			unsigned j;

			if (sym->bind != MELF_STB_GLOBAL || sym->shndx == MELF_SHN_UNDEF) {
				continue;
			}
			for (j = 0; j < n; j++) {
				if (strcmp(names[j], sym->name) == 0) {
					names[j] = names[--n];
					j--;
				}
			}
		}
	}
	*count = n;
	return names;
}

static bool defines_any(struct melf *e, char **names, unsigned n)
{
	unsigned i;
	unsigned j;

	for (i = 1; i < e->nsymbols; i++) {
		struct melf_symbol *sym = &e->symbols[i];

		if (sym->bind != MELF_STB_GLOBAL || sym->shndx == MELF_SHN_UNDEF) {
			continue;
		}
		for (j = 0; j < n; j++) {
			if (strcmp(names[j], sym->name) == 0) {
				return true;
			}
		}
	}
	return false;
}

static void add_input(const char *path, struct melf *e)
{
	inputs = xrealloc(inputs, (ninputs + 1) * sizeof *inputs);
	memset(&inputs[ninputs], 0, sizeof *inputs);
	inputs[ninputs].path = path;
	inputs[ninputs].e = e;
	merge_input(&inputs[ninputs]);
	ninputs++;
}

/* Load archive members until no member defines anything still undefined.
 * A member loaded for one symbol may need others, hence the passes. */
static void load_from_archives(void)
{
	bool changed;

	do {
		unsigned n;
		char **names = undefined_names(&n);
		unsigned k;

		changed = false;
		for (k = 0; k < narchives && n > 0; k++) {
			struct archive *a = &archives[k];
			unsigned i;

			for (i = 0; i < a->nmembers; i++) {
				struct member *m = &a->members[i];
				char err[128];
				struct melf *e;
				char *path;

				if (m->loaded) {
					continue;
				}
				e = melf_read_mem(m->data, m->size, err, sizeof err);
				if (e == NULL) {
					error(a->path, "%s: %s", m->name, err);
					m->loaded = true;
					continue;
				}
				if (e->e_type != MELF_ET_REL || !defines_any(e, names, n)) {
					melf_free(e);
					continue;
				}
				path = xalloc(strlen(a->path) + strlen(m->name) + 3);
				sprintf(path, "%s(%s)", a->path, m->name);
				add_input(path, e);
				m->loaded = true;
				changed = true;
				free(names);
				names = undefined_names(&n);
			}
		}
		free(names);
	} while (changed);
}

int main(int argc, char *argv[])
{
	const char *output = NULL;
	const char *format = NULL;
	const char *map = NULL;
	const char *entry_name = NULL;
	uint32_t base = 0;
	uint32_t data_base = 0;
	bool have_data_base = false;
	uint32_t entry;
	bool elf, cfx = false;
	const char *reloc_list = NULL;
	int i;
	unsigned k;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			inputs = xrealloc(inputs, (ninputs + 1) * sizeof *inputs);
			memset(&inputs[ninputs], 0, sizeof *inputs);
			inputs[ninputs++].path = a;
			continue;
		}
		if (strcmp(a, "-B") == 0) {
			bss_backwards = true;
			continue;
		}
		if (strcmp(a, "-p") == 0) {
			pad_flat = true;
			continue;
		}
		if (strlen(a) == 2 && strchr("ofbdMeSR", a[1]) != NULL) {
			const char *v;

			if (i + 1 == argc) {
				usage();
			}
			v = argv[++i];
			switch (a[1]) {
			case 'o': output = v; break;
			case 'f': format = v; break;
			case 'b': base = (uint32_t)strtoul(v, NULL, 0); break;
			case 'd':
				data_base = (uint32_t)strtoul(v, NULL, 0);
				have_data_base = true;
				break;
			case 'M': map = v; break;
			case 'e': entry_name = v; break;
			case 'S':
				symfiles = xrealloc(symfiles, (nsymfiles + 1) * sizeof *symfiles);
				symfiles[nsymfiles++] = v;
				break;
			case 'R': reloc_list = v; break;
			}
			continue;
		}
		usage();
	}
	if (ninputs == 0) {
		usage();
	}
	if (output == NULL) {
		output = "a.out";
	}
	if (format == NULL) {
		elf = has_suffix(output, ".bin") == false &&
		      has_suffix(output, ".rom") == false;
	} else if (strcmp(format, "elf") == 0) {
		elf = true;
	} else if (strcmp(format, "bin") == 0) {
		elf = false;
	} else if (strcmp(format, "cfx") == 0) {
		elf = false;
		cfx = true;
		base = 0;
		have_data_base = false;
	} else {
		usage();
	}
	for (k = 0; k < ninputs; k++) {
		char err[128];

		if (is_archive(inputs[k].path)) {
			read_archive(inputs[k].path);
			memmove(&inputs[k], &inputs[k + 1], (ninputs - k - 1) * sizeof *inputs);
			ninputs--;
			k--;
			continue;
		}
		inputs[k].e = melf_read(inputs[k].path, err, sizeof err);
		if (inputs[k].e == NULL) {
			error(inputs[k].path, "%s", err);
			continue;
		}
		if (inputs[k].e->e_type != MELF_ET_REL) {
			error(inputs[k].path, "not a relocatable object");
			continue;
		}
		merge_input(&inputs[k]);
	}
	if (error_count == 0) {
		load_from_archives();
	}
	if (error_count > 0) {
		return 1;
	}
	if (bss_backwards == true) {
		reverse_bss();
	}
	layout(base, have_data_base, data_base);
	collect_globals();
	define_layout_symbols(elf == false);
	resolve_from_symfiles();
	check_undefined();
	for (k = 0; k < ninputs; k++) {
		apply_relocs(&inputs[k]);
	}
	entry = find_entry(entry_name);
	if (error_count > 0) {
		return 1;
	}
	if (elf == true) {
		write_elf(output, entry);
	} else if (cfx == true) {
		write_cfx(output, entry);
	} else {
		write_flat(output, base);
	}
	if (error_count == 0 && reloc_list != NULL) {
		write_reloc_list(reloc_list);
	}
	if (error_count == 0 && map != NULL) {
		FILE *f = strcmp(map, "-") == 0 ? stdout : fopen(map, "w");

		if (f == NULL) {
			error(NULL, "cannot write '%s'", map);
		} else {
			write_map(f);
			if (f != stdout) {
				fclose(f);
			}
		}
	}
	if (error_count > 0) {
		remove(output);
		return 1;
	}
	return 0;
}
