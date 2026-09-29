#include <stdlib.h>
#include <string.h>

#include "mas.h"

static void usage(void)
{
	fputs("usage: mas [-o output] [-f bin|elf] [-b base] [-l listing] [-M map]\n"
	      "           [-I dir] [-D name[=value]] [-Werror] source...\n",
	      stderr);
	exit(2);
}

static bool has_suffix(const char *s, const char *suffix)
{
	size_t n = strlen(s);
	size_t m = strlen(suffix);

	return n >= m && strcmp(s + n - m, suffix) == 0;
}

int main(int argc, char *argv[])
{
	const char *output = NULL;
	const char *format = NULL;
	const char *listing = NULL;
	const char *map = NULL;
	uint32_t base = 0;
	int i;
	int ninputs = 0;
	bool elf;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (a[0] != '-' || a[1] == '\0') {
			ninputs++;
			continue;
		}
		if (strcmp(a, "-Werror") == 0) {
			warnings_are_errors = true;
			continue;
		}
		if (strlen(a) == 2 && strchr("ofblMID", a[1]) != NULL) {
			const char *v;

			if (i + 1 == argc) {
				usage();
			}
			v = argv[++i];
			switch (a[1]) {
			case 'o': output = v; break;
			case 'f': format = v; break;
			case 'b': base = (uint32_t)strtoul(v, NULL, 0); break;
			case 'l': listing = v; break;
			case 'M': map = v; break;
			case 'I': add_include_dir(v); break;
			case 'D': assemble_define(v); break;
			}
			continue;
		}
		usage();
	}
	if (ninputs == 0) {
		usage();
	}
	if (output == NULL) {
		output = "out.bin";
	}
	if (format == NULL) {
		elf = has_suffix(output, ".o") || has_suffix(output, ".elf");
	} else if (strcmp(format, "elf") == 0) {
		elf = true;
	} else if (strcmp(format, "bin") == 0) {
		elf = false;
	} else {
		usage();
	}
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (a[0] == '-' && a[1] != '\0') {
			if (strcmp(a, "-Werror") != 0) {
				i++;
			}
			continue;
		}
		assemble_file(a);
	}
	layout();
	emit();
	if (error_count == 0) {
		if (elf == true) {
			write_elf(output);
		} else {
			write_flat(output, base);
		}
	}
	if (error_count == 0 && listing != NULL) {
		FILE *f = strcmp(listing, "-") == 0 ? stdout : fopen(listing, "w");

		if (f == NULL) {
			fatal("cannot write '%s'", listing);
		}
		write_listing(f);
		if (f != stdout) {
			fclose(f);
		}
	}
	if (error_count == 0 && map != NULL) {
		FILE *f = strcmp(map, "-") == 0 ? stdout : fopen(map, "w");

		if (f == NULL) {
			fatal("cannot write '%s'", map);
		}
		write_map(f);
		if (f != stdout) {
			fclose(f);
		}
	}
	if (error_count > 0) {
		remove(output);
		return 1;
	}
	return 0;
}
