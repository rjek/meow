#include "msim.h"

/* what tests/fuzz found: each function is a random program cut down to
 * the expression that was compiled wrongly; all of these faults but the
 * structure copy were in the compiler's machine-independent parts */
static void show(unsigned n) { print_hex(n); putchar('\n'); }

static unsigned g[8];
static unsigned short gs[8];
static int out[4];
static int calls;
static int side(int v) { calls++; return v; }

struct S { int a; short b; signed char c; unsigned char u8; unsigned short u16;
           unsigned d; long long e; double f; int arr[6]; struct S *next; };
static struct S s[4];
static double gd[4];
static int budget;

/* a relation compared with a constant other than 0 or 1 is not the
 * relation or its opposite; and a constant arm of ?: compared with a
 * constant is known only for that arm */
static unsigned rel_const(unsigned x, unsigned y, int c)
{
    unsigned r = 0;
    r = r * 3 + (203 != ((int)y >= (int)x));
    r = r * 3 + (203 == ((int)y >= (int)x));
    r = r * 3 + (1 != ((int)y >= (int)x));
    r = r * 3 + (0 == ((int)y >= (int)x));
    r = r * 3 + (2 == (x && y));
    r = r * 3 + (-1 != (x || y));
    r = r * 3 + (2 != (side(x) < side(y)));
    r = r * 3 + ((c ? 5 : x) == 5);
    r = r * 3 + ((c ? x : 5) == 5);
    r = r * 3 + ((c ? 5 : x) != 5);
    r = r * 3 + ((c ? x : 5) != 5);
    if (2 == (x < y)) r += 1000;
    if (2 != (x < y)) r += 5000;
    return r;
}

/* (-a) * n for negative n is a * -n, not a * n */
static unsigned neg_times(unsigned x, unsigned y)
{
    return (((x - y) * 0) - 92) * 0xfa0e7542u;
}

/* (a / k) / k1 is a / (k * k1) only while k * k1 is a value of the type */
static unsigned div_div(unsigned a, int b)
{
    return (a / 133) / 0x9813358fu + (a / 0xdd513b6bu) / 0xfffffbffu
         + (unsigned)((b / 65536) / 65536) + (unsigned)((b / -46341) / 46341)
         + (a / 1000) / 1000 + (unsigned)((b / -1000) / 1000);
}

/* shifts that add up to the width of the type or more */
static unsigned long_shift(unsigned a, unsigned b)
{
    return b >> (((a << 16 >> 17) >> 29) & 31);
}

/* ?: lifted out of a loop: what one arm computed (here x, or y) is not
 * there for the code after it to use */
static unsigned lift_arm(unsigned a, unsigned b)
{
    unsigned x = a ^ 0x40000000u, y = b + 0x15c766c5u;
    int i;
    for (i = 0; i < 3; i++)
        g[b & 7] = ~(0x80000000u > x + g[i & 7]
                       ? ((13 != b ? y : x) != 235 ? (a | x) : y - 0x0feb5132u)
                       : g[5] ^ a);
    return g[b & 7];
}

/* ?: within ?:, lifted out of a loop */
static unsigned lift_nested(unsigned a, unsigned b, unsigned d)
{
    unsigned x = a ^ 29, y = b + 67;
    int i;
    for (i = 0; i < 5; i++)
        out[i & 3] = (int)(172 >= b ? (10 ^ a) >> (b & 31)
                                    : (int)(b >> 4) <= (int)(gs[x & 7] + y)) / (int)d;
    return out[0];
}

/* ?: as the second arm of ?:, lifted: its value goes where the first
 * arm's went */
static unsigned lift_second(unsigned a, unsigned b, struct S *p)
{
    unsigned x = a ^ 2484, y = b + 10;
    int i;
    if (budget > 0) { budget--; b = lift_second(b, (unsigned)(short)(((a | b) << (y & 31)) ^ ~(unsigned)((int)a / (int)((p->d & 1023) + 1))), &s[2]); }
    switch ((((unsigned)p->arr[1] + 0x9f989ca5u) - (a - x)) % 6) {
    case 5:
        for (i = 0; i < 4; i++)
            b = (unsigned)((int)(((y | a) << (x & 31)) ^ (b >> ((b ^ 213) & 31))) / (int)(((((a & gs[a & 7]) & p->u16) != (unsigned)p->a ? ((int)y <= (int)(unsigned)(9 >> (p->next->a & 31))) : x << 30 >> 20 << 6 >> 8) & 1023) + 1));
    }
    return x ^ (y << 3) ^ a ^ b;
}

/* a structure copy long enough to be a loop changes the flags, so the
 * copy both arms start with cannot move between the compare and branch */
static unsigned copy_flags(unsigned a, unsigned b, struct S *p, unsigned c)
{
    unsigned x = a ^ 115;
    if (c >= b) {
        s[1] = *p; p = p->next;
        a ^= x;
    } else {
        s[1] = *p; p = p->next;
    }
    return a ^ p->a;
}

/* the load of gs[x & 7] is known to fetch the 7 just stored, and is also
 * a common subexpression whose value, in a function this busy, is kept on
 * the stack: it must be put there */
static unsigned f2(unsigned a, unsigned b, struct S *p) { return a ^ b ^ (unsigned)p->a; }

static unsigned stored_const(unsigned a, unsigned b, struct S *p)
{
    unsigned x = a ^ 12, y = b + 5;
    switch (((((p->d & b) + (p->d - p->next->d)) - (unsigned)(signed char)g[1]) & (((g[b & 7] - 0x7ef00000u) >= y ? (1 % (a | 1)) : (unsigned)((int)x >> (b & 31))) - ((b | y) / ((x | y) | 1)))) % 4) {
    case 1:
        gs[x & 7] = 7;
        switch (((x + (gs[x & 7] + (b + b))) / 15) % 9) {
        case 5:
            p->f = p->f * 1.25 + (double)(int)(((((gs[x & 7] | (unsigned)s[1].a) << ((g[0] - a) & 31)) - y) - (unsigned)((int)((gs[y & 7] / (b | 1)) + (0x4000u & x)) / (int)((((b + y) + (b & (unsigned)s[1].arr[3])) & 1023) + 1))) & 0xffff) - gd[3] / 7.0;
            gd[3] = p->f + 21.0; y += (unsigned)(int)p->f + (p->f < gd[0]);
        }
    }
    if (budget > 0) { budget--; return f2(x, y, p); }
    return x ^ (y << 3) ^ a ^ b;
}

int main(void)
{
    unsigned acc = 0;
    int i, j;

    for (i = 0; i < 16; i++) acc = acc * 31 + rel_const(i & 3 ? 5 : 9, i & 2 ? 5 : 7, i & 8);
    show(acc); show(calls);
    show(neg_times(3, 4));
    show(div_div(0xffffffffu, 0x7fffffff)); show(div_div(0xfffffbffu, -0x7fffffff - 1)); show(div_div(2000000, -2000000));
    show(long_shift(0x78bbc8aa, 0x36c47a78)); show(long_shift(0xffffffffu, 0x80000000u));
    for (i = 0; i < 8; i++) g[i] = i * 0x01010101u;
    show(lift_arm(1, 13)); show(lift_arm(0xc0000000u, 13)); show(lift_arm(7, 2)); show(lift_arm(235 - 0x15c766c5u, 4));
    show(lift_nested(100, 3, 1)); show(lift_nested(100, 300, 1)); show(lift_nested(100, 0xf0000000u, 1));
    for (i = 0; i < 4; i++) {
        s[i].next = &s[(i + 1) & 3]; s[i].a = i * 77; s[i].d = i + 0x1000; s[i].f = i * 1.5;
        for (j = 0; j < 6; j++) s[i].arr[j] = i * j;
    }
    acc = 0;
    for (i = 0; i < 3; i++) acc = acc * 31 + copy_flags(acc + i, i * 3416, &s[i & 3], 3000);
    show(acc);
    acc = 0;
    for (i = 0; i < 5; i++) { budget = 31; acc = acc * 31 + lift_second(acc + i, i * 0x1700000u, &s[i & 3]); }
    show(acc);
    for (i = 0; i < 8; i++) g[i] = 0;
    show(stored_const(0x49a4f37f, 0xf4, &s[2]));
    acc = 0;
    for (i = 0; i < 400; i++) { budget = i & 1; acc = acc * 31 + stored_const(acc + i, i * 122, &s[i & 3]); }
    show(acc);
    return 0;
}
