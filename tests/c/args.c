#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

struct small { int a; };
struct mid { int a, b, c; };
struct big { int v[6]; char tag; };

int six(int a, int b, int c, int d, int e, int f) { return a - b + c - d + e - f; }
int mixed(int a, struct mid m, int b, struct small s, struct big g, int c)
{
    return a * 1000000 + m.a * 100000 + m.b * 10000 + m.c * 1000 + b * 100 + s.a * 10 + g.v[5] + g.tag + c;
}
struct big makebig(int seed)
{
    struct big g;
    int i;
    for (i = 0; i < 6; i++) g.v[i] = seed + i;
    g.tag = 'q';
    return g;
}
struct mid swapmid(struct mid m) { struct mid r; r.a = m.c; r.b = m.b; r.c = m.a; return r; }
int sumbig(struct big g) { int i, s = g.tag; for (i = 0; i < 6; i++) s += g.v[i]; return s; }

int apply(int (*f)(int, int, int, int, int, int), int base)
{
    int r = 0;
    int i;
    for (i = 0; i < 3; i++) r += f(base + i, i, base, 1, 2, i * 3);
    return r;
}

int main(void)
{
    struct mid m = { 1, 2, 3 };
    struct small s = { 7 };
    struct big g = makebig(10);
    struct mid t;

    show(six(1, 2, 3, 4, 5, 6));
    show(six(100, -100, 100, -100, 100, -100));
    show(mixed(9, m, 8, s, g, 5) - 'q');
    show(sumbig(g));
    show(sumbig(makebig(-3)));
    t = swapmid(m);
    show(t.a * 100 + t.b * 10 + t.c);
    t = swapmid(swapmid(t));
    show(t.a * 100 + t.b * 10 + t.c);
    show(apply(six, 10));
    return 0;
}
