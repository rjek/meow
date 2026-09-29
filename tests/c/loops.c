#include "msim.h"

/* loops the strength reducer must get right: every shape of index and
 * stride, indices used for other things, variables live after the loop */
static void show(int n) { print_int(n); putchar('\n'); }

int a[40], b[40];
unsigned char c[64];
short h[32];
struct rec { int x; char t; short s; } r[16];
int g2[6][8];

int sum_after(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) s += a[i];
    return s + i;                       /* i lives on */
}

int two_loops(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) s += a[i];
    for (i = 0; i < n; i += 3) s += b[i] * 2;
    return s;
}

int index_used(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) { a[i] = i * 3; s += a[i] + i; }
    return s;
}

int down(int n)
{
    int i, s = 0;
    for (i = n - 1; i >= 0; i--) s = s * 3 + a[i];
    return s;
}

int stride(int n, int k)
{
    int i, s = 0;
    for (i = 0; i < n; i += k) s += c[i];
    return s;
}

int with_continue(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) { if (a[i] & 1) continue; s += a[i]; }
    return s;
}

int with_break(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) { if (a[i] > 50) break; s += b[i]; }
    return s + i;
}

int nested(void)
{
    int i, j, s = 0;
    for (i = 0; i < 6; i++)
        for (j = 0; j < 8; j++) { g2[i][j] = i * j; s += g2[i][j] + g2[j % 6][i]; }
    return s;
}

int modified_base(int n)
{
    int *p = a, i, s = 0;
    for (i = 0; i < n; i++) { s += p[i]; if (i == 3) p = b; }
    return s;
}

int modified_index(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) { s += a[i]; if (a[i] == 9) i++; }
    return s;
}

unsigned unsigned_index(unsigned n)
{
    unsigned i, s = 0;
    for (i = 0; i < n; i++) s = s * 7 + h[i];
    return s;
}

int structs(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) { r[i].x = i; r[i].t = (char)(i * 5); r[i].s = (short)(i * 300); }
    for (i = n - 1; i >= 0; i -= 2) s += r[i].x + r[i].t + r[i].s;
    return s;
}

int conditional_access(int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++) s += (i & 1) ? a[i] : b[i];
    return s;
}

int two_arrays_one_index(int n)
{
    int i;
    for (i = 0; i < n; i++) b[i] = a[i] + a[n - 1 - i];
    return b[0] + b[n - 1];
}

int comma_init(int n)
{
    int i, j, s = 0;
    for (i = 0, j = n; i < n; i++, j--) s += a[i] * j;
    return s;
}

int not_equal_test(int n)
{
    int i, s = 0;
    for (i = 0; i != n; i++) s += c[i];
    return s;
}

int char_loop(void)
{
    int i, s = 0;
    for (i = 0; i < 64; i++) c[i] = (unsigned char)(i * 7);
    for (i = 63; i > 0; i -= 5) s += c[i];
    return s;
}

int main(void)
{
    int i;
    for (i = 0; i < 40; i++) { a[i] = (i * 37) % 61; b[i] = 100 - i; }
    for (i = 0; i < 32; i++) h[i] = (short)(i * 1000 - 5000);
    for (i = 0; i < 64; i++) c[i] = (unsigned char)(i * 13);
    show(sum_after(10)); show(sum_after(0)); show(two_loops(40));
    show(index_used(20)); show(down(12)); show(stride(64, 7)); show(stride(64, 1));
    show(with_continue(40)); show(with_break(40)); show(nested());
    show(modified_base(10)); show(modified_index(40)); show((int)unsigned_index(32));
    show(structs(16)); show(conditional_access(40)); show(two_arrays_one_index(9));
    show(comma_init(40)); show(not_equal_test(50)); show(char_loop());
    return 0;
}
