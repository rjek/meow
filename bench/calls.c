#include "bench.h"

/* call and return: recursion with small frames and a few arguments */
static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
static int tak(int x, int y, int z) { return y < x ? tak(tak(x - 1, y, z), tak(y - 1, z, x), tak(z - 1, x, y)) : z; }
static int ack(int m, int n) { return m == 0 ? n + 1 : n == 0 ? ack(m - 1, 1) : ack(m - 1, ack(m, n - 1)); }
static int six(int a, int b, int c, int d, int e, int f) { return a + b - c + d - e + f; }
static int leaf(int a) { return a * 2 + 1; }

int main(void)
{
    unsigned sum = 0;
    int i;

    sum += fib(20);
    sum = sum * 3 + tak(14, 10, 6);
    sum = sum * 3 + ack(2, 300);
    for (i = 0; i < 20000; i++) sum += six(i, 1, 2, 3, 4, leaf(i));
    report("calls", sum);
    return 0;
}
