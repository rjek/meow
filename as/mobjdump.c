/* mobjdump: print the sections, symbols and relocations of a MEOW ELF file. */
#include <stdio.h>
#include <stdlib.h>

#include "meow.h"
#include "melf.h"

static const char *reloc_name(unsigned type)
{
	switch (type) {
	case R_MEOW_NONE: return "NONE";
	case R_MEOW_ABS32: return "ABS32";
	case R_MEOW_ABS16: return "ABS16";
	case R_MEOW_ABS8: return "ABS8";
	case R_MEOW_REL32: return "REL32";
	default: return "?";
	}
}

int main(int argc, char *argv[])
{
	struct melf *e;
	char err[128];
	unsigned i;
	bool disassemble = false;
	const char *path;

	if (argc == 3 && argv[1][0] == '-' && argv[1][1] == 'd') {
		disassemble = true;
		path = argv[2];
	} else if (argc == 2) {
		path = argv[1];
	} else {
		fputs("usage: mobjdump [-d] file.o\n", stderr);
		return 2;
	}
	e = melf_read(path, err, sizeof err);
	if (e == NULL) {
		fprintf(stderr, "mobjdump: %s: %s\n", path, err);
		return 1;
	}
	printf("%s: %s, entry 0x%08x\n", path,
	       e->e_type == MELF_ET_EXEC ? "executable" : "relocatable", e->entry);
	puts("Sections");
	for (i = 1; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];

		if (s->type != MELF_SHT_PROGBITS && s->type != MELF_SHT_NOBITS) {
			continue;
		}
		printf("  %2u %-16s %s%s%s addr 0x%08x size 0x%x align %u\n", i,
		       s->name, (s->flags & MELF_SHF_ALLOC) != 0 ? "A" : "-",
		       (s->flags & MELF_SHF_WRITE) != 0 ? "W" : "-",
		       (s->flags & MELF_SHF_EXECINSTR) != 0 ? "X" : "-",
		       s->addr, s->size, s->align);
	}
	puts("Symbols");
	for (i = 1; i < e->nsymbols; i++) {
		struct melf_symbol *s = &e->symbols[i];
		char where[16];

		if (s->shndx == MELF_SHN_UNDEF) {
			snprintf(where, sizeof where, "UND");
		} else if (s->shndx == MELF_SHN_ABS) {
			snprintf(where, sizeof where, "ABS");
		} else {
			snprintf(where, sizeof where, "%d", s->shndx);
		}
		printf("  %3u 0x%08x %-6s %-4s %-7s %s\n", i, s->value,
		       s->bind == MELF_STB_GLOBAL ? "GLOBAL" : "LOCAL",
		       s->type == MELF_STT_FUNC ? "FUNC" :
		       s->type == MELF_STT_SECTION ? "SECT" :
		       s->type == MELF_STT_OBJECT ? "OBJ" : "NONE",
		       where, s->name);
	}
	for (i = 1; i < e->nsections; i++) {
		unsigned j;

		if (e->nrelas[i] == 0) {
			continue;
		}
		printf("Relocations for %s\n", e->sections[i].name);
		for (j = 0; j < e->nrelas[i]; j++) {
			struct melf_rela *r = &e->relas[i][j];
			const char *sym = (unsigned)r->sym < e->nsymbols ?
					  e->symbols[r->sym].name : "?";

			if (sym[0] == '\0' && (unsigned)r->sym < e->nsymbols &&
			    e->symbols[r->sym].type == MELF_STT_SECTION) {
				sym = e->sections[e->symbols[r->sym].shndx].name;
			}
			printf("  0x%08x %-6s %s%+d\n", r->offset, reloc_name(r->type),
			       sym, (int)r->addend);
		}
	}
	if (disassemble == true) {
		for (i = 1; i < e->nsections; i++) {
			struct melf_section *s = &e->sections[i];
			uint32_t off;

			if (s->type != MELF_SHT_PROGBITS ||
			    (s->flags & MELF_SHF_EXECINSTR) == 0) {
				continue;
			}
			printf("Disassembly of %s\n", s->name);
			for (off = 0; off + 1 < s->size; off += 2) {
				char buf[64];
				uint16_t w = (uint16_t)(s->data[off] | (s->data[off + 1] << 8));

				meow_disasm(w, s->addr + off, buf, sizeof buf);
				printf("  %08x %04x  %s\n", s->addr + off, w, buf);
			}
		}
	}
	melf_free(e);
	return 0;
}
