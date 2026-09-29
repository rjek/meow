#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "melf.h"

#define EHDR_SIZE 52
#define SHDR_SIZE 40
#define SYM_SIZE 16
#define RELA_SIZE 12

static void *xalloc(size_t n)
{
	void *p = calloc(1, n == 0 ? 1 : n);

	if (p == NULL) {
		fputs("melf: out of memory\n", stderr);
		exit(2);
	}
	return p;
}

static char *dup(const char *s)
{
	char *p = xalloc(strlen(s) + 1);

	strcpy(p, s);
	return p;
}

struct melf *melf_new(uint16_t e_type)
{
	struct melf *e = xalloc(sizeof *e);

	e->e_type = e_type;
	melf_add_section(e, "", 0, 0, NULL, 0, 0);	/* index 0 */
	melf_add_symbol(e, "", 0, MELF_STB_LOCAL, MELF_STT_NOTYPE,
			MELF_SHN_UNDEF);
	return e;
}

void melf_free(struct melf *e)
{
	unsigned i;

	if (e == NULL) {
		return;
	}
	for (i = 0; i < e->nsections; i++) {
		free(e->sections[i].name);
		free(e->sections[i].data);
		if (e->relas != NULL) {
			free(e->relas[i]);
		}
	}
	for (i = 0; i < e->nsymbols; i++) {
		free(e->symbols[i].name);
	}
	free(e->sections);
	free(e->symbols);
	free(e->relas);
	free(e->nrelas);
	free(e);
}

int melf_add_section(struct melf *e, const char *name, uint32_t type,
		     uint32_t flags, const uint8_t *data, uint32_t size,
		     uint32_t align)
{
	struct melf_section *s;
	unsigned i = e->nsections;

	e->sections = realloc(e->sections, (i + 1) * sizeof *e->sections);
	e->relas = realloc(e->relas, (i + 1) * sizeof *e->relas);
	e->nrelas = realloc(e->nrelas, (i + 1) * sizeof *e->nrelas);
	if (e->sections == NULL || e->relas == NULL || e->nrelas == NULL) {
		fputs("melf: out of memory\n", stderr);
		exit(2);
	}
	s = &e->sections[i];
	memset(s, 0, sizeof *s);
	s->name = dup(name);
	s->type = type;
	s->flags = flags;
	s->size = size;
	s->align = align;
	s->index = (int)i;
	if (data != NULL && type != MELF_SHT_NOBITS) {
		s->data = xalloc(size);
		memcpy(s->data, data, size);
	}
	e->relas[i] = NULL;
	e->nrelas[i] = 0;
	e->nsections++;
	return (int)i;
}

int melf_add_symbol(struct melf *e, const char *name, uint32_t value,
		    unsigned bind, unsigned type, int shndx)
{
	struct melf_symbol *s;
	unsigned i = e->nsymbols;

	e->symbols = realloc(e->symbols, (i + 1) * sizeof *e->symbols);
	if (e->symbols == NULL) {
		fputs("melf: out of memory\n", stderr);
		exit(2);
	}
	s = &e->symbols[i];
	memset(s, 0, sizeof *s);
	s->name = dup(name);
	s->value = value;
	s->bind = bind;
	s->type = type;
	s->shndx = shndx;
	if (bind == MELF_STB_LOCAL) {
		e->first_global = i + 1;
	}
	e->nsymbols++;
	return (int)i;
}

void melf_add_rela(struct melf *e, int section, uint32_t offset, int sym,
		   unsigned type, int32_t addend)
{
	unsigned n = e->nrelas[section];
	struct melf_rela *r;

	e->relas[section] = realloc(e->relas[section],
				    (n + 1) * sizeof *e->relas[section]);
	if (e->relas[section] == NULL) {
		fputs("melf: out of memory\n", stderr);
		exit(2);
	}
	r = &e->relas[section][n];
	r->offset = offset;
	r->sym = sym;
	r->type = type;
	r->addend = addend;
	e->nrelas[section]++;
}

/* ---- writer ----------------------------------------------------------- */

struct strtab {
	char *data;
	uint32_t size;
	uint32_t cap;
};

static uint32_t strtab_add(struct strtab *t, const char *s)
{
	size_t n = strlen(s) + 1;
	uint32_t off;

	if (t->size + n > t->cap) {
		t->cap = (t->cap + (uint32_t)n) * 2;
		t->data = realloc(t->data, t->cap);
		if (t->data == NULL) {
			fputs("melf: out of memory\n", stderr);
			exit(2);
		}
	}
	if (t->size == 0) {
		t->data[0] = '\0';
		t->size = 1;
		if (s[0] == '\0') {
			return 0;
		}
	}
	off = t->size;
	memcpy(t->data + off, s, n);
	t->size += (uint32_t)n;
	return off;
}

static void put16(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
	put16(p, v);
	put16(p + 2, v >> 16);
}

static void write_shdr(FILE *f, uint32_t name, uint32_t type, uint32_t flags,
		       uint32_t addr, uint32_t off, uint32_t size,
		       uint32_t link, uint32_t info, uint32_t align,
		       uint32_t entsize)
{
	uint8_t h[SHDR_SIZE];

	put32(h + 0, name);
	put32(h + 4, type);
	put32(h + 8, flags);
	put32(h + 12, addr);
	put32(h + 16, off);
	put32(h + 20, size);
	put32(h + 24, link);
	put32(h + 28, info);
	put32(h + 32, align);
	put32(h + 36, entsize);
	fwrite(h, 1, sizeof h, f);
}

static uint32_t align_up(uint32_t v, uint32_t a)
{
	return a > 1 ? (v + a - 1) & ~(a - 1) : v;
}

bool melf_fwrite(struct melf *e, FILE *f)
{
	struct strtab shstr = { NULL, 0, 0 };
	struct strtab str = { NULL, 0, 0 };
	uint8_t *symtab;
	unsigned i;
	unsigned nrela_secs = 0;
	unsigned total;
	uint32_t off = EHDR_SIZE;
	uint32_t *sec_off;
	uint32_t *rela_off;
	uint32_t *rela_name;
	uint32_t symtab_off;
	uint32_t strtab_off;
	uint32_t shstr_off;
	uint32_t shoff;
	uint32_t symtab_name;
	uint32_t strtab_name;
	uint32_t shstr_name;
	int symtab_index;
	uint8_t ehdr[EHDR_SIZE];

	if (f == NULL) {
		return false;
	}
	strtab_add(&shstr, "");
	strtab_add(&str, "");
	for (i = 0; i < e->nsections; i++) {
		e->sections[i].name_off = strtab_add(&shstr, e->sections[i].name);
		if (e->nrelas[i] > 0) {
			nrela_secs++;
		}
	}
	symtab = xalloc((size_t)e->nsymbols * SYM_SIZE);
	for (i = 0; i < e->nsymbols; i++) {
		struct melf_symbol *s = &e->symbols[i];
		uint8_t *p = symtab + i * SYM_SIZE;

		put32(p + 0, strtab_add(&str, s->name));
		put32(p + 4, s->value);
		put32(p + 8, s->size);
		p[12] = (uint8_t)((s->bind << 4) | (s->type & 15));
		p[13] = 0;
		put16(p + 14, (uint32_t)s->shndx);
	}
	/* file layout: header, section data, rela tables, symtab, strtab,
	 * shstrtab, section headers */
	sec_off = xalloc(e->nsections * sizeof *sec_off);
	rela_off = xalloc(e->nsections * sizeof *rela_off);
	rela_name = xalloc(e->nsections * sizeof *rela_name);
	for (i = 1; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];

		off = align_up(off, s->align > 1 ? s->align : 1);
		sec_off[i] = off;
		if (s->type != MELF_SHT_NOBITS) {
			off += s->size;
		}
	}
	for (i = 1; i < e->nsections; i++) {
		if (e->nrelas[i] > 0) {
			char name[256];

			snprintf(name, sizeof name, ".rela%s", e->sections[i].name);
			rela_name[i] = strtab_add(&shstr, name);
			off = align_up(off, 4);
			rela_off[i] = off;
			off += e->nrelas[i] * RELA_SIZE;
		}
	}
	off = align_up(off, 4);
	symtab_off = off;
	symtab_name = strtab_add(&shstr, ".symtab");
	off += e->nsymbols * SYM_SIZE;
	strtab_off = off;
	strtab_name = strtab_add(&shstr, ".strtab");
	off += str.size;
	shstr_name = strtab_add(&shstr, ".shstrtab");
	shstr_off = off;
	off += shstr.size;
	shoff = align_up(off, 4);
	total = e->nsections + nrela_secs + 3;
	symtab_index = (int)(e->nsections + nrela_secs);

	memset(ehdr, 0, sizeof ehdr);
	memcpy(ehdr, "\177ELF\1\1\1", 7);
	put16(ehdr + 16, e->e_type);
	put16(ehdr + 18, EM_MEOW);
	put32(ehdr + 20, 1);
	put32(ehdr + 24, e->entry);
	put32(ehdr + 28, 0);
	put32(ehdr + 32, shoff);
	put32(ehdr + 36, 0);
	put16(ehdr + 40, EHDR_SIZE);
	put16(ehdr + 42, 0);
	put16(ehdr + 44, 0);
	put16(ehdr + 46, SHDR_SIZE);
	put16(ehdr + 48, total);
	put16(ehdr + 50, total - 1);
	fwrite(ehdr, 1, sizeof ehdr, f);

	for (i = 1; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];

		if (s->type == MELF_SHT_NOBITS) {
			continue;
		}
		while ((uint32_t)ftell(f) < sec_off[i]) {
			fputc(0, f);
		}
		if (s->size > 0) {
			fwrite(s->data, 1, s->size, f);
		}
	}
	for (i = 1; i < e->nsections; i++) {
		unsigned j;

		if (e->nrelas[i] == 0) {
			continue;
		}
		while ((uint32_t)ftell(f) < rela_off[i]) {
			fputc(0, f);
		}
		for (j = 0; j < e->nrelas[i]; j++) {
			struct melf_rela *r = &e->relas[i][j];
			uint8_t buf[RELA_SIZE];

			put32(buf + 0, r->offset);
			put32(buf + 4, ((uint32_t)r->sym << 8) | (r->type & 0xff));
			put32(buf + 8, (uint32_t)r->addend);
			fwrite(buf, 1, sizeof buf, f);
		}
	}
	while ((uint32_t)ftell(f) < symtab_off) {
		fputc(0, f);
	}
	fwrite(symtab, 1, (size_t)e->nsymbols * SYM_SIZE, f);
	fwrite(str.data, 1, str.size, f);
	fwrite(shstr.data, 1, shstr.size, f);
	while ((uint32_t)ftell(f) < shoff) {
		fputc(0, f);
	}
	for (i = 0; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];

		write_shdr(f, s->name_off, s->type, s->flags, s->addr,
			   i == 0 ? 0 : sec_off[i], s->size, 0, 0, s->align, 0);
	}
	for (i = 1; i < e->nsections; i++) {
		if (e->nrelas[i] > 0) {
			write_shdr(f, rela_name[i], 4, 0x40, 0, rela_off[i],
				   e->nrelas[i] * RELA_SIZE, (uint32_t)symtab_index,
				   i, 4, RELA_SIZE);
		}
	}
	write_shdr(f, symtab_name, 2, 0, 0, symtab_off, e->nsymbols * SYM_SIZE,
		   (uint32_t)symtab_index + 1, e->first_global, 4, SYM_SIZE);
	write_shdr(f, strtab_name, 3, 0, 0, strtab_off, str.size, 0, 0, 1, 0);
	write_shdr(f, shstr_name, 3, 0, 0, shstr_off, shstr.size, 0, 0, 1, 0);
	free(symtab);
	free(sec_off);
	free(rela_off);
	free(rela_name);
	free(shstr.data);
	free(str.data);
	return ferror(f) == 0;
}

bool melf_write(struct melf *e, const char *path)
{
	FILE *f = fopen(path, "wb");
	bool ok;

	if (f == NULL) {
		return false;
	}
	ok = melf_fwrite(e, f);
	if (fclose(f) != 0) {
		ok = false;
	}
	return ok;
}

/* ---- reader ----------------------------------------------------------- */

static uint32_t get16(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
	return get16(p) | (get16(p + 2) << 16);
}

static bool fail(char *errbuf, size_t errlen, const char *msg)
{
	snprintf(errbuf, errlen, "%s", msg);
	return false;
}

struct melf *melf_read(const char *path, char *errbuf, size_t errlen)
{
	FILE *f = fopen(path, "rb");
	long len;
	uint8_t *img;
	struct melf *e;
	uint32_t shoff;
	uint32_t shnum;
	uint32_t shstrndx;
	unsigned i;
	const uint8_t *shstr;
	uint32_t shstr_size;

	if (f == NULL) {
		fail(errbuf, errlen, "cannot open");
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < EHDR_SIZE) {
		fclose(f);
		fail(errbuf, errlen, "too short to be ELF");
		return NULL;
	}
	img = xalloc((size_t)len);
	if (fread(img, 1, (size_t)len, f) != (size_t)len) {
		fclose(f);
		free(img);
		fail(errbuf, errlen, "short read");
		return NULL;
	}
	fclose(f);
	if (memcmp(img, "\177ELF\1\1\1", 7) != 0) {
		free(img);
		fail(errbuf, errlen, "not a 32-bit little-endian ELF file");
		return NULL;
	}
	if (get16(img + 18) != EM_MEOW) {
		free(img);
		fail(errbuf, errlen, "not a MEOW ELF file");
		return NULL;
	}
	shoff = get32(img + 32);
	shnum = get16(img + 48);
	shstrndx = get16(img + 50);
	if (get16(img + 46) != SHDR_SIZE || shoff + shnum * SHDR_SIZE > (uint32_t)len ||
	    shstrndx >= shnum) {
		free(img);
		fail(errbuf, errlen, "bad section header table");
		return NULL;
	}
	e = xalloc(sizeof *e);
	e->e_type = (uint16_t)get16(img + 16);
	e->entry = get32(img + 24);
	{
		const uint8_t *h = img + shoff + shstrndx * SHDR_SIZE;

		shstr = img + get32(h + 16);
		shstr_size = get32(h + 20);
		if (get32(h + 16) + shstr_size > (uint32_t)len) {
			free(img);
			melf_free(e);
			fail(errbuf, errlen, "bad section name table");
			return NULL;
		}
	}
	for (i = 0; i < shnum; i++) {
		const uint8_t *h = img + shoff + i * SHDR_SIZE;
		uint32_t name = get32(h + 0);
		uint32_t type = get32(h + 4);
		uint32_t off = get32(h + 16);
		uint32_t size = get32(h + 20);
		int idx;

		if (name >= shstr_size || (type != MELF_SHT_NOBITS &&
					   off + size > (uint32_t)len)) {
			free(img);
			melf_free(e);
			fail(errbuf, errlen, "bad section header");
			return NULL;
		}
		idx = melf_add_section(e, (const char *)shstr + name, type,
				       get32(h + 8), type == MELF_SHT_NOBITS ?
				       NULL : img + off, size, get32(h + 32));
		e->sections[idx].addr = get32(h + 12);
		e->sections[idx].link = (int)get32(h + 24);
		e->sections[idx].info = (int)get32(h + 28);
		e->sections[idx].entsize = get32(h + 36);
		e->sections[idx].file_off = off;
	}
	/* symbols */
	for (i = 0; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];
		struct melf_section *strs;
		unsigned n;
		unsigned j;

		if (s->type != 2) {
			continue;
		}
		if (s->link < 0 || (unsigned)s->link >= e->nsections) {
			break;
		}
		strs = &e->sections[s->link];
		n = s->size / SYM_SIZE;
		for (j = 0; j < n; j++) {
			const uint8_t *p = s->data + j * SYM_SIZE;
			uint32_t name = get32(p);
			int idx;

			if (name >= strs->size) {
				name = 0;
			}
			idx = melf_add_symbol(e, (const char *)strs->data + name,
					      get32(p + 4), p[12] >> 4, p[12] & 15,
					      (int)get16(p + 14));
			e->symbols[idx].size = get32(p + 8);
		}
		e->first_global = (unsigned)s->info;
		break;
	}
	/* relocations */
	for (i = 0; i < e->nsections; i++) {
		struct melf_section *s = &e->sections[i];
		unsigned n;
		unsigned j;

		if (s->type != 4 || s->info < 0 || (unsigned)s->info >= e->nsections) {
			continue;
		}
		n = s->size / RELA_SIZE;
		for (j = 0; j < n; j++) {
			const uint8_t *p = s->data + j * RELA_SIZE;
			uint32_t info = get32(p + 4);

			melf_add_rela(e, s->info, get32(p), (int)(info >> 8),
				      info & 0xff, (int32_t)get32(p + 8));
		}
	}
	free(img);
	return e;
}
