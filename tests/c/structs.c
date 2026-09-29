#include "msim.h"

struct point { int x, y; };
struct rec { char tag; short n; int v; unsigned char b[3]; struct point p; };

static void show(int n) { print_int(n); putchar('\n'); }

struct point make(int x, int y) { struct point p; p.x = x; p.y = y; return p; }
int manhattan(struct point p) { return (p.x < 0 ? -p.x : p.x) + (p.y < 0 ? -p.y : p.y); }
void bump(struct rec *r) { r->n += 1; r->v += r->b[1]; r->p.x -= 2; r->tag = 'z'; }

struct rec table[3] = {
    { 'a', 10, 100, { 1, 2, 3 }, { 5, 6 } },
    { 'b', 20, 200, { 4, 5, 6 }, { 7, 8 } },
    { 'c', 30, 300, { 7, 8, 9 }, { 9, 10 } },
};

int main(void)
{
    struct point q = make(3, -4);
    struct rec local;
    struct rec *r;
    int i;

    show(sizeof(struct rec));
    show(q.x * 10 + q.y);
    show(manhattan(q));
    show(manhattan(make(-7, 9)));
    local = table[1];
    bump(&local);
    show(local.tag);
    show(local.n);
    show(local.v);
    show(local.p.x);
    for (i = 0; i < 3; i++) {
        r = &table[i];
        bump(r);
        show(r->tag + r->n + r->v + r->b[0] + r->b[2] + r->p.x + r->p.y);
    }
    show(table[2].n);
    return 0;
}
