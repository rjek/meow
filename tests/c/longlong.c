#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }
static void showll(long long v)
{
    print_hex((unsigned)(v >> 32)); putchar(':'); print_hex((unsigned)v); putchar('\n');
}

long long add(long long a, long long b) { return a + b; }
long long sub(long long a, long long b) { return a - b; }
long long neg(long long a) { return -a; }
long long shl(long long a, int n) { return a << n; }
long long shr(long long a, int n) { return a >> n; }
unsigned long long ushr(unsigned long long a, int n) { return a >> n; }
long long band(long long a, long long b) { return a & b; }
int cmp(long long a, long long b) { return a < b ? -1 : a > b ? 1 : 0; }
int ucmp(unsigned long long a, unsigned long long b) { return a < b ? -1 : a > b ? 1 : 0; }
long long widen(int a) { return a; }
unsigned long long uwiden(unsigned a) { return a; }
int narrow(long long a) { return (int)a; }

long long table[3] = { 0x0123456789abcdefLL, -1LL, 0x8000000000000000LL };

int main(void)
{
    long long a = 0x7fffffffLL;
    long long b = 1;
    int i;

    showll(add(a, b));
    showll(add(table[0], table[1]));
    showll(sub(b, a));
    showll(sub(0, table[0]));
    showll(neg(table[2]));
    showll(shl(a, 33));
    showll(shl(table[0], 4));
    showll(shr(table[2], 60));
    showll(ushr(table[2], 60));
    showll(shr(table[0], 36));
    showll(band(table[0], -256LL));
    show(cmp(a, b));
    show(cmp(table[1], table[0]));
    show(cmp(table[2], table[1]));
    show(ucmp(table[2], table[1]));
    show(ucmp(table[1], table[2]));
    showll(widen(-5));
    showll(uwiden(0xfffffff0u));
    show(narrow(table[0]));
    for (i = 0; i < 3; i++) {
        table[i] += 0x100000000LL;
        showll(table[i]);
    }
    return 0;
}
