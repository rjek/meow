#include "msim.h"

/* division and remainder by constants: the compiler multiplies by a
 * reciprocal instead of calling the library */
static void show(int n) { print_int(n); putchar('\n'); }
static void showu(unsigned n) { print_int((int)(n / 10)); print_int((int)(n % 10)); putchar('\n'); }

unsigned u3(unsigned x) { return x / 3; }
unsigned u7r(unsigned x) { return x % 7; }
unsigned u10(unsigned x) { return x / 10; }
unsigned u100000(unsigned x) { return x / 100000u; }
unsigned u641(unsigned x) { return x / 641; }
unsigned ubig(unsigned x) { return x / 0x80000001u; }
unsigned u25(unsigned x) { return x / 25 + x % 25; }
int s3(int x) { return x / 3; }
int s7r(int x) { return x % 7; }
int sm5(int x) { return x / -5; }
int sm7r(int x) { return x % -7; }
int s1000(int x) { return x / 1000; }
int s12r(int x) { return x % 12; }

int main(void)
{
    static const unsigned uv[] = { 0, 1, 2, 3, 9, 10, 99, 100, 640, 641, 12345, 99999, 100000,
        0x7fffffffu, 0x80000000u, 0x80000001u, 0xdeadbeefu, 0xffffffffu };
    static const int sv[] = { 0, 1, 2, 3, -1, -2, -3, 6, -6, 7, -7, 99, -99, 1000, -1000, 999,
        -999, 12345, -12345, 2147483647, -2147483647, -2147483647 - 1 };
    unsigned i;

    for (i = 0; i < sizeof uv / sizeof uv[0]; i++) {
        unsigned x = uv[i];
        showu(u3(x)); showu(u7r(x)); showu(u10(x)); showu(u100000(x));
        showu(u641(x)); showu(ubig(x)); showu(u25(x));
    }
    for (i = 0; i < sizeof sv / sizeof sv[0]; i++) {
        int x = sv[i];
        show(s3(x)); show(s7r(x)); show(sm5(x)); show(sm7r(x)); show(s1000(x)); show(s12r(x));
    }
    return 0;
}
