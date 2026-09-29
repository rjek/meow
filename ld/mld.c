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
	uint32_t addr;
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

static struct input *inputs;
static unsigned ninputs;
static struct osec **osecs;
static unsigned nosecs;
static struct gsym *gsyms;
static unsigned ngsyms;
static int error_count;

static void usage(void)
{
	fputs("usage: mld [-o output] [-f elf|bin] [-b base] [-M map] [-e entry]\n"
	      "           input.o...\n", stderr);
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
	} else if ((flags & MELF_SHF_EXECINSTR) != 0) {
		s->order = 0;
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
static void layout(uint32_t base)
{
	struct osec **sorted = xalloc(nosecs * sizeof *sorted);
	unsigned n = 0;
	unsigned i;
	int order;
	uint32_t addr = base;

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
		addr = align_up(addr, o->align);
		o->addr = addr;
		addr += o->size;
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
		}
	}
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
		while (pos < o->addr) {
			fputc(0, f);
			pos++;
		}
		fwrite(o->data, 1, o->size, f);
		pos += o->size;
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
		fprintf(f, "  %08x %8u %s\n", osecs[i]->addr, osecs[i]->size,
			osecs[i]->name);
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

int main(int argc, char *argv[])
{
	const char *output = NULL;
	const char *format = NULL;
	const char *map = NULL;
	const char *entry_name = NULL;
	uint32_t base = 0;
	uint32_t entry;
	bool elf;
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
		if (strlen(a) == 2 && strchr("ofbMe", a[1]) != NULL) {
			const char *v;

			if (i + 1 == argc) {
				usage();
			}
			v = argv[++i];
			switch (a[1]) {
			case 'o': output = v; break;
			case 'f': format = v; break;
			case 'b': base = (uint32_t)strtoul(v, NULL, 0); break;
			case 'M': map = v; break;
			case 'e': entry_name = v; break;
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
	} else {
		usage();
	}
	for (k = 0; k < ninputs; k++) {
		char err[128];

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
	if (error_count > 0) {
		return 1;
	}
	layout(base);
	collect_globals();
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
	} else {
		write_flat(output, base);
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
