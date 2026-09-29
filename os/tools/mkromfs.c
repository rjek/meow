/* mkromfs: pack a directory tree into a romfs image for Catflap.
 *
 *   mkromfs image directory
 *
 * The image is a header, a table of entries sorted by full path, the
 * names, then the file data, everything little-endian and word aligned.
 * Directories are entries with no data.  See os/kernel/romfs.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>

#define MAGIC "catflap1"

struct entry {
	char *path;		/* relative, no leading slash */
	int dir;
	uint32_t size;
	uint32_t name_off, data_off;
	unsigned char *data;
};

static struct entry *entries;
static unsigned nentries;

static void fail(const char *what, const char *name)
{
	fprintf(stderr, "mkromfs: %s %s\n", what, name);
	exit(1);
}

static void add(const char *root, const char *rel)
{
	char full[4096];
	struct stat st;
	struct entry *e;

	snprintf(full, sizeof full, "%s/%s", root, rel);
	if (stat(full, &st) != 0) {
		fail("cannot stat", full);
	}
	entries = realloc(entries, (nentries + 1) * sizeof *entries);
	e = &entries[nentries++];
	memset(e, 0, sizeof *e);
	e->path = strdup(rel);
	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(full);
		struct dirent *de;

		e->dir = 1;
		if (d == NULL) {
			fail("cannot open", full);
		}
		while ((de = readdir(d)) != NULL) {
			char sub[4096];

			if (de->d_name[0] == '.') {
				continue;
			}
			snprintf(sub, sizeof sub, "%s/%s", rel, de->d_name);
			add(root, sub);
		}
		closedir(d);
	} else {
		FILE *f = fopen(full, "rb");

		if (f == NULL) {
			fail("cannot read", full);
		}
		e->size = (uint32_t)st.st_size;
		e->data = malloc(e->size + 1);
		if (fread(e->data, 1, e->size, f) != e->size) {
			fail("short read of", full);
		}
		fclose(f);
	}
}

static int by_path(const void *a, const void *b)
{
	return strcmp(((const struct entry *)a)->path, ((const struct entry *)b)->path);
}

static void put32(FILE *f, uint32_t v)
{
	fputc(v & 0xff, f);
	fputc((v >> 8) & 0xff, f);
	fputc((v >> 16) & 0xff, f);
	fputc((v >> 24) & 0xff, f);
}

static void pad(FILE *f, uint32_t *pos)
{
	while (*pos % 4 != 0) {
		fputc(0, f);
		(*pos)++;
	}
}

int main(int argc, char **argv)
{
	FILE *out;
	DIR *d;
	struct dirent *de;
	uint32_t names = 0, data = 0, pos;
	unsigned i;

	if (argc != 3) {
		fprintf(stderr, "usage: mkromfs image directory\n");
		return 2;
	}
	d = opendir(argv[2]);
	if (d == NULL) {
		fail("cannot open", argv[2]);
	}
	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] != '.') {
			add(argv[2], de->d_name);
		}
	}
	closedir(d);
	qsort(entries, nentries, sizeof *entries, by_path);
	for (i = 0; i < nentries; i++) {
		entries[i].name_off = names;
		names += (uint32_t)strlen(entries[i].path) + 1;
	}
	names = (names + 3) & ~3u;
	for (i = 0; i < nentries; i++) {
		if (entries[i].dir == 0) {
			entries[i].data_off = data;
			data += (entries[i].size + 3) & ~3u;
		}
	}
	out = fopen(argv[1], "wb");
	if (out == NULL) {
		fail("cannot write", argv[1]);
	}
	/* header: magic, entries, names size, data size */
	fwrite(MAGIC, 1, 8, out);
	put32(out, nentries);
	put32(out, names);
	put32(out, data);
	for (i = 0; i < nentries; i++) {
		put32(out, entries[i].name_off);
		put32(out, entries[i].dir ? 0xffffffffu : entries[i].data_off);
		put32(out, entries[i].size);
		put32(out, 0);
	}
	pos = 0;
	for (i = 0; i < nentries; i++) {
		fputs(entries[i].path, out);
		fputc(0, out);
		pos += (uint32_t)strlen(entries[i].path) + 1;
	}
	pad(out, &pos);
	pos = 0;
	for (i = 0; i < nentries; i++) {
		if (entries[i].dir == 0) {
			fwrite(entries[i].data, 1, entries[i].size, out);
			pos += entries[i].size;
			pad(out, &pos);
		}
	}
	fclose(out);
	printf("mkromfs: %u entries, %u bytes of data\n", nentries, data);
	return 0;
}
