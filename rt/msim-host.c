/* Host implementation of the msim console calls, so a C test's expected
 * output can be produced by the host compiler. */
#include <stdio.h>
#include <stdlib.h>

#undef putchar
#undef puts

int putchar(int c) { return fputc(c, stdout); }
int puts(const char *s) { fputs(s, stdout); fputc('\n', stdout); return 0; }
void print_int(int n) { printf("%d", n); }
void print_hex(unsigned n) { printf("%x", n); }
