/* Minimal ELF32 little-endian writer and reader for MEOW objects. */
#ifndef MELF_H
#define MELF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define EM_MEOW 0x4d45		/* private machine number, "ME" */

/* Relocation types. */
#define R_MEOW_NONE  0
#define R_MEOW_ABS32 1
#define R_MEOW_ABS16 2
#define R_MEOW_ABS8  3
#define R_MEOW_REL32 4

/* Standard constants we use, so callers need not include <elf.h>. */
#define MELF_ET_REL  1
#define MELF_ET_EXEC 2
#define MELF_SHT_PROGBITS 1
#define MELF_SHT_NOBITS 8
#define MELF_SHF_WRITE 1
#define MELF_SHF_ALLOC 2
#define MELF_SHF_EXECINSTR 4
#define MELF_STB_LOCAL 0
#define MELF_STB_GLOBAL 1
#define MELF_STT_NOTYPE 0
#define MELF_STT_OBJECT 1
#define MELF_STT_FUNC 2
#define MELF_STT_SECTION 3
#define MELF_SHN_UNDEF 0
#define MELF_SHN_ABS 0xfff1

struct melf_section {
	char *name;
	uint32_t type;
	uint32_t flags;
	uint32_t addr;
	uint32_t align;
	uint8_t *data;
	uint32_t size;
	int link;		/* for relocation sections: symtab */
	int info;		/* for relocation sections: target */
	uint32_t entsize;
	int index;
	uint32_t name_off;
	uint32_t file_off;
};

struct melf_symbol {
	char *name;
	uint32_t value;
	uint32_t size;
	unsigned bind;
	unsigned type;
	int shndx;		/* section index, MELF_SHN_UNDEF or MELF_SHN_ABS */
	uint32_t name_off;
};

struct melf_rela {
	uint32_t offset;
	int sym;		/* index into the symbol table */
	unsigned type;
	int32_t addend;
};

struct melf {
	uint16_t e_type;
	uint32_t entry;
	struct melf_section *sections;
	unsigned nsections;
	struct melf_symbol *symbols;	/* locals first */
	unsigned nsymbols;
	unsigned first_global;
	struct melf_rela **relas;	/* per section, may be NULL */
	unsigned *nrelas;
};

struct melf *melf_new(uint16_t e_type);
void melf_free(struct melf *e);

/* Sections and symbols return their index. */
int melf_add_section(struct melf *e, const char *name, uint32_t type,
		     uint32_t flags, const uint8_t *data, uint32_t size,
		     uint32_t align);
int melf_add_symbol(struct melf *e, const char *name, uint32_t value,
		    unsigned bind, unsigned type, int shndx);
void melf_add_rela(struct melf *e, int section, uint32_t offset, int sym,
		   unsigned type, int32_t addend);

/* Write a relocatable or executable file.  Symbols are reordered so that
 * locals precede globals; melf_symbol_index maps an add-time index to
 * the final one. */
bool melf_write(struct melf *e, const char *path);
bool melf_fwrite(struct melf *e, FILE *f);

/* Read a MEOW ELF file into the same structure.  Section data is loaded;
 * relocations are decoded.  Returns NULL with a message in errbuf on
 * failure. */
struct melf *melf_read(const char *path, char *errbuf, size_t errlen);
/* The same from an image already in memory, such as an archive member.
 * The image is not kept. */
struct melf *melf_read_mem(const uint8_t *img, size_t len, char *errbuf,
			   size_t errlen);

#endif /* MELF_H */
