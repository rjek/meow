/* mdis: disassemble a flat MEOW binary. */
#include <stdio.h>
#include <stdlib.h>

#include "meow.h"

int main(int argc, char *argv[])
{
	FILE *f;
	uint32_t base = 0;
	uint32_t pc;
	int lo;
	int hi;

	if (argc < 2 || argc > 3) {
		fputs("usage: mdis file.bin [base]\n", stderr);
		return 2;
	}
	f = fopen(argv[1], "rb");
	if (f == NULL) {
		perror(argv[1]);
		return 1;
	}
	if (argc == 3) {
		base = (uint32_t)strtoul(argv[2], NULL, 0);
	}
	pc = base;
	while ((lo = getc(f)) != EOF) {
		char buf[64];
		uint16_t w;

		hi = getc(f);
		if (hi == EOF) {
			printf("%08x %02x       DCB 0x%02x\n", pc, lo, lo);
			break;
		}
		w = (uint16_t)(lo | (hi << 8));
		meow_disasm(w, pc, buf, sizeof buf);
		printf("%08x %04x     %s\n", pc, w, buf);
		pc += 2;
	}
	fclose(f);
	return 0;
}
