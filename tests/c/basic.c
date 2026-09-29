#include "msim.h"

int glob = 5;
unsigned char bytes[4] = { 1, 2, 3, 250 };
short shorts[3] = { -1, 300, 32767 };

static void show(int n) { print_int(n); putchar('\n'); }
static void showx(unsigned n) { print_hex(n); putchar('\n'); }

int add(int a, int b) { return a + b; }

int sum(int *p, int n)
{
    int s = 0;
    int i;
    for (i = 0; i < n; i++) s += p[i];
    return s;
}

int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }

int many(int a, int b, int c, int d, int e, int f)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f;
}

const char *name(int n)
{
    switch (n) {
    case 0: return "zero";
    case 1: return "one";
    case 2: return "two";
    case 3: return "three";
    case 7: return "seven";
    default: return "many";
    }
}

int main(void)
{
    int table[5] = { 10, 20, 30, 40, 50 };
    unsigned u = 0xf0000000u;
    int i;

    show(add(glob, 2));                  /* 7 */
    show(sum(table, 5));                 /* 150 */
    show(fact(6));                       /* 720 */
    show(many(1, 2, 3, 4, 5, 6));        /* 91 */
    show(glob * 3 + add(glob, 2));       /* 22 */
    show(100 / 7);                       /* 14 */
    show(100 % 7);                       /* 2 */
    show(-100 / 7);                      /* -14 */
    show(-100 % 7);                      /* -2 */
    show(u / 16);                        /* 251658240 */
    showx(u >> 4);                        /* f000000 */
    show(bytes[3] + bytes[0]);           /* 251 */
    show(shorts[0] + shorts[1]);         /* 299 */
    show(u > 1 ? 1 : 0);                 /* 1: unsigned compare */
    show((int)u > 1 ? 1 : 0);            /* 0: signed compare */
    for (i = 0; i < 8; i++) puts(name(i));
    return 3;
}
