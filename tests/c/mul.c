#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }
static void showx(unsigned n) { print_hex(n); putchar('\n'); }

int m3(int x) { return x * 3; }
int m10(int x) { return x * 10; }
int m12(int x) { return x * 12; }
int m100(int x) { return x * 100; }
int mneg7(int x) { return x * -7; }
int m1000(int x) { return x * 1000; }
int m65537(int x) { return x * 65537; }
unsigned um(unsigned a, unsigned b) { return a * b; }
int d3(int x) { return x / 3; }
int r10(int x) { return x % 10; }
unsigned ud(unsigned a, unsigned b) { return a / b; }
unsigned ur(unsigned a, unsigned b) { return a % b; }

int main(void)
{
    int v[6] = { 0, 1, -1, 7, 1000, -12345 };
    int i;
    for (i = 0; i < 6; i++) {
        show(m3(v[i])); show(m10(v[i])); show(m12(v[i])); show(m100(v[i]));
        show(mneg7(v[i])); show(m1000(v[i])); show(m65537(v[i]));
        show(d3(v[i])); show(r10(v[i]));
    }
    show(um(123456, 7890));
    showx(um(0x12345678u, 0x9abcdef0u));
    show(ud(0xffffffffu, 10));
    show(ur(0xffffffffu, 10));
    show(ud(1000000, 1));
    show(ur(7, 0x80000000u));
    show(-2147483647 - 1);
    show((-2147483647 - 1) / -1 == 0 ? 0 : 1);
    return 0;
}
