/* Output: flat binary, ELF relocatable, listing and map. */
#include <stdlib.h>
#include <string.h>

#include "mas.h"
#include "melf.h"

static bool flat_mode;

/* Absolute value of a symbol in flat mode. */
static bool symbol_address(struct symbol *s, uint32_t *out)
{
	struct value v;
	struct expr *e = expr_symbol(s->name, &s->def);

	if (expr_eval(e, NULL, false, &v) == false || v.ext != NULL) {
		return false;
	}
	*out = (uint32_t)(v.v + (v.sec != NULL ? v.sec->base : 0));
	return true;
}

static void store_le(uint8_t *p, uint32_t v, unsigned width)
{
	unsigned i;

	for (i = 0; i < width; i++) {
		p[i] = (uint8_t)(v >> (8 * i));
	}
}

static unsigned reloc_width(unsigned type)
{
	switch (type) {
	case R_MEOW_ABS8: return 1;
	case R_MEOW_ABS16: return 2;
	default: return 4;
	}
}

static void apply_relocs(void)
{
	struct section *s;

	for (s = sec_first(); s != NULL; s = s->next) {
		struct reloc *r;

		for (r = s->relocs; r != NULL; r = r->next) {
			uint32_t v = (uint32_t)r->addend;
			unsigned w = reloc_width(r->type);

			if (r->sym != NULL) {
				uint32_t sv;

				if (r->sym->kind == SYM_UNDEFINED ||
				    symbol_address(r->sym, &sv) == false) {
					error_at(&r->loc, "undefined symbol '%s'",
						 r->sym->name);
					continue;
				}
				v += sv;
			} else if (r->sec != NULL) {
				v += r->sec->base;
			}
			if (w < 4 && (v >> (8 * w)) != 0 &&
			    (int32_t)v >> (8 * w) != -1) {
				error_at(&r->loc, "value 0x%x does not fit in %u bytes",
					 v, w);
			}
			store_le(s->data + r->offset, v, w);
		}
	}
}

static void assign_bases(uint32_t base)
{
	struct section *s;
	uint32_t addr = base;

	for (s = sec_first(); s != NULL; s = s->next) {
		addr = (addr + s->align - 1) & ~(s->align - 1);
		s->base = addr;
		addr += s->size;
	}
}

void write_flat(const char *path, uint32_t base)
{
	FILE *f;
	struct section *s;
	struct section *last_data = NULL;
	uint32_t pos;

	flat_mode = true;
	assign_bases(base);
	apply_relocs();
	if (error_count > 0) {
		return;
	}
	for (s = sec_first(); s != NULL; s = s->next) {
		if (s->kind != SEC_BSS && s->size > 0) {
			last_data = s;
		}
	}
	f = fopen(path, "wb");
	if (f == NULL) {
		fatal("cannot write '%s'", path);
	}
	pos = base;
	for (s = sec_first(); s != NULL; s = s->next) {
		while (pos < s->base) {
			fputc(0, f);
			pos++;
		}
		if (s->kind != SEC_BSS) {
			fwrite(s->data, 1, s->size, f);
			pos += s->size;
		} else if (last_data != NULL && s->base < last_data->base) {
			uint32_t i;

			for (i = 0; i < s->size; i++) {
				fputc(0, f);
			}
			pos += s->size;
		}
		if (s == last_data) {
			break;
		}
	}
	if (fclose(f) != 0) {
		fatal("write error on '%s'", path);
	}
}

static bool elf_symbol_value(struct symbol *s, uint32_t *value, int *shndx)
{
	struct value v;
	struct expr *e;

	switch (s->kind) {
	case SYM_LABEL:
		*value = s->anchor->addr;
		*shndx = s->sec->index;
		return true;
	case SYM_EQU:
		e = expr_symbol(s->name, &s->def);
		if (expr_eval(e, NULL, true, &v) == false || v.ext != NULL) {
			return false;
		}
		*value = (uint32_t)v.v;
		*shndx = v.sec != NULL ? v.sec->index : MELF_SHN_ABS;
		return true;
	case SYM_UNDEFINED:
		*value = 0;
		*shndx = MELF_SHN_UNDEF;
		return true;
	default:
		return false;
	}
}

void write_elf(const char *path)
{
	struct melf *e = melf_new(MELF_ET_REL);
	struct section *s;
	struct symbol *sym;
	int pass;

	for (s = sec_first(); s != NULL; s = s->next) {
		uint32_t flags = MELF_SHF_ALLOC;
		uint32_t type = s->kind == SEC_BSS ? MELF_SHT_NOBITS
						   : MELF_SHT_PROGBITS;

		if (s->readonly == false) {
			flags |= MELF_SHF_WRITE;
		}
		if (s->kind == SEC_CODE) {
			flags |= MELF_SHF_EXECINSTR;
		}
		s->index = melf_add_section(e, s->name, type, flags, s->data,
					    s->size, s->align);
	}
	/* locals first, then globals, as ELF requires */
	for (s = sec_first(); s != NULL; s = s->next) {
		s->sym->index = melf_add_symbol(e, "", 0, MELF_STB_LOCAL,
						MELF_STT_SECTION, s->index);
	}
	for (pass = 0; pass < 2; pass++) {
		for (sym = sym_first(); sym != NULL; sym = sym->list) {
			bool global = sym->exported == true ||
				      sym->kind == SYM_UNDEFINED;
			uint32_t value;
			int shndx;

			if (sym->kind == SYM_SECTION || sym->kind == SYM_REGISTER ||
			    (sym->kind == SYM_UNDEFINED && sym->refs == 0)) {
				continue;
			}
			if ((pass == 0) == global) {
				continue;
			}
			if (sym->kind == SYM_UNDEFINED && sym->imported == false) {
				error_at(sym->def.file != NULL ? &sym->def : NULL,
					 "undefined symbol '%s'", sym->name);
				continue;
			}
			if (sym->exported == true && sym->kind == SYM_UNDEFINED) {
				error_at(NULL, "exported symbol '%s' is not defined",
					 sym->name);
				continue;
			}
			if (elf_symbol_value(sym, &value, &shndx) == false) {
				continue;
			}
			if (strchr(sym->name, '\001') != NULL) {
				continue;	/* macro-scoped local label */
			}
			sym->index = melf_add_symbol(e, sym->name, value,
						     global == true ? MELF_STB_GLOBAL
								    : MELF_STB_LOCAL,
						     sym->kind == SYM_LABEL &&
						     sym->sec->kind == SEC_CODE ?
						     MELF_STT_FUNC : MELF_STT_NOTYPE,
						     shndx);
		}
	}
	for (s = sec_first(); s != NULL; s = s->next) {
		struct reloc *r;

		for (r = s->relocs; r != NULL; r = r->next) {
			int symi;

			if (r->sym != NULL) {
				symi = r->sym->index;
			} else {
				symi = r->sec->sym->index;
			}
			melf_add_rela(e, s->index, r->offset, symi, r->type,
				      (int32_t)r->addend);
		}
	}
	if (entry_symbol != NULL) {
		uint32_t value;
		int shndx;

		if (elf_symbol_value(entry_symbol, &value, &shndx) == true) {
			e->entry = value;
		}
	}
	if (error_count == 0 && melf_write(e, path) == false) {
		fatal("cannot write '%s'", path);
	}
	melf_free(e);
}

/* ---- listing ---------------------------------------------------------- */

static void list_item(FILE *f, struct item *it, const char *text)
{
	uint32_t addr = it->sec->base + it->addr;
	uint32_t n = it->size;
	uint32_t off = 0;

	do {
		char bytes[32] = "";
		uint32_t k;
		size_t len = 0;

		for (k = 0; k < 8 && off + k < n; k++) {
			len += (size_t)snprintf(bytes + len, sizeof bytes - len,
						"%02x", it->sec->kind == SEC_BSS ?
						0 : it->sec->data[it->addr + off + k]);
		}
		fprintf(f, "%08x %-16s %s\n", addr + off, bytes,
			text != NULL ? text : "");
		text = NULL;
		off += 8;
	} while (off < n);
}

void write_listing(FILE *f)
{
	struct section *s;
	struct item **first;
	struct item **tail;
	int i;

	first = xcalloc((size_t)listing_count + 1, sizeof *first);
	tail = xcalloc((size_t)listing_count + 1, sizeof *tail);
	for (s = sec_first(); s != NULL; s = s->next) {
		struct item *it;

		for (it = s->items; it != NULL; it = it->next) {
			int line = it->list_line;

			if (line < 0 || line >= listing_count || it->size == 0) {
				continue;
			}
			if (first[line] == NULL) {
				first[line] = it;
			} else {
				tail[line]->list_next = it;
			}
			tail[line] = it;
		}
	}
	for (i = 0; i < listing_count; i++) {
		struct item *it = first[i];
		const char *text = listing_lines[i].text;

		if (it == NULL) {
			fprintf(f, "%-26s%s\n", "", text);
			continue;
		}
		for (; it != NULL; it = it->list_next) {
			list_item(f, it, text);
			text = NULL;
		}
	}
	free(first);
	free(tail);
}

void write_map(FILE *f)
{
	struct section *s;
	struct symbol *sym;

	fputs("Sections\n", f);
	for (s = sec_first(); s != NULL; s = s->next) {
		fprintf(f, "  %08x %8u %s\n", s->base, s->size, s->name);
	}
	fputs("Symbols\n", f);
	for (sym = sym_first(); sym != NULL; sym = sym->list) {
		uint32_t v;
		const char *tag;

		if (sym->kind == SYM_SECTION || sym->kind == SYM_REGISTER ||
		    strchr(sym->name, '\001') != NULL) {
			continue;
		}
		if (sym->kind == SYM_UNDEFINED) {
			fprintf(f, "  %8s %s (undefined)\n", "", sym->name);
			continue;
		}
		if (symbol_address(sym, &v) == false) {
			continue;
		}
		tag = sym->exported == true ? " (exported)" : "";
		fprintf(f, "  %08x %s%s\n", v, sym->name, tag);
	}
}
