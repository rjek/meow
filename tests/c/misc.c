#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

int sparse(int n)
{
    switch (n) {
    case -1000: return 1;
    case -5: return 2;
    case 0: return 3;
    case 17: return 4;
    case 100: return 5;
    case 1000: return 6;
    case 65536: return 7;
    case 0x7fffffff: return 8;
    }
    return 0;
}

unsigned shifts(unsigned v, int n) { return (v << n) | (v >> (32 - n)); }
int sshift(int v, int n) { return v >> n; }
int logic(int a, int b, int c) { return (a && b) || (!c && a != b) ? 1 : 0; }
int comma(int a) { int b; return (b = a + 1, b * 2); }
unsigned char narrow(unsigned v) { return (unsigned char)(v >> 4); }
short snarrow(int v) { return (short)v; }
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
int gcd(int a, int b) { while (b != 0) { int t = a % b; a = b; b = t; } return a; }
int strlen_(const char *s) { const char *p = s; while (*p) p++; return p - s; }
int cmpfn(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

void sort(int *v, int n, int (*cmp)(const void *, const void *))
{
    int i, j;
    for (i = 1; i < n; i++) {
        int t = v[i];
        for (j = i; j > 0 && cmp(&v[j - 1], &t) > 0; j--) v[j] = v[j - 1];
        v[j] = t;
    }
}

int labels(int n)
{
    int c = 0;
again:
    if (n <= 1) goto out;
    n = (n & 1) ? 3 * n + 1 : n / 2;
    c++;
    goto again;
out:
    return c;
}

int main(void)
{
    static const int probe[] = { -1000, -5, 0, 17, 100, 1000, 65536, 0x7fffffff, 1, -1, 99 };
    int v[8] = { 5, -3, 9, 0, 22, -100, 7, 7 };
    int i;
    for (i = 0; i < 11; i++) show(sparse(probe[i]));
    print_hex(shifts(0x80000001u, 4)); putchar('\n');
    show(sshift(-256, 4));
    show(logic(1, 0, 1) * 4 + logic(0, 1, 0) * 2 + logic(2, 3, 0));
    show(comma(20));
    show(narrow(0xabcdu));
    show(snarrow(0x18000));
    show(fib(15));
    show(gcd(1071, 462));
    show(strlen_("twelve chars"));
    sort(v, 8, cmpfn);
    for (i = 0; i < 8; i++) { print_int(v[i]); putchar(' '); }
    putchar('\n');
    show(labels(27));
    return fib(5);
}
