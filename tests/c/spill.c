#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

/* more live values than registers */
int pressure(int a, int b, int c, int d, int e, int f, int g, int h)
{
    int i = a + b, j = b + c, k = c + d, l = d + e, m = e + f, n = f + g, o = g + h;
    int p = a * b, q = c * d, r = e * f, s = g * h;
    int t;
    for (t = 0; t < 3; t++) {
        i += j ^ k; j += k ^ l; k += l ^ m; l += m ^ n; m += n ^ o; n += o ^ i; o += i ^ j;
        p -= q; q -= r; r -= s; s -= p;
    }
    return i + j + k + l + m + n + o + p + q + r + s + a + b + c + d + e + f + g + h;
}

struct bits { unsigned a : 3; unsigned b : 5; signed int c : 4; unsigned d : 20; };
union u { int i; unsigned char b[4]; short s[2]; };

int bitsum(struct bits *p) { return p->a + p->b + p->c + p->d; }

static int counter(void) { static int n = 100; return n++; }

/* a frame bigger than any load offset */
int bigframe(int n)
{
    unsigned char buf[5000];
    int words[300];
    int i, s = 0;
    for (i = 0; i < 5000; i++) buf[i] = (unsigned char)(i * 7);
    for (i = 0; i < 300; i++) words[i] = i * i;
    for (i = 0; i < 5000; i += 97) s += buf[i];
    for (i = 0; i < 300; i += 13) s += words[i];
    return s + buf[4999] + words[299] + n;
}

int grid[4][5];

int main(void)
{
    struct bits bf;
    union u un;
    int i, j;

    show(pressure(1, 2, 3, 4, 5, 6, 7, 8));
    show(pressure(-9, 8, -7, 6, -5, 4, -3, 2));
    bf.a = 5; bf.b = 31; bf.c = -3; bf.d = 1000000;
    show(bitsum(&bf));
    bf.c = 7; bf.a += 3;
    show(bf.a); show(bf.c); show(bitsum(&bf));
    un.i = 0x04030201;
    show(un.b[0] + un.b[3] * 10);
    show(un.s[1]);
    un.s[0] = -1;
    print_hex((unsigned)un.i); putchar('\n');
    show(counter() + counter() * 1000);
    show(bigframe(3));
    for (i = 0; i < 4; i++) for (j = 0; j < 5; j++) grid[i][j] = i * 10 + j;
    for (i = 0; i < 4; i++) show(grid[i][4 - i] + grid[3 - i][i]);
    return 0;
}
