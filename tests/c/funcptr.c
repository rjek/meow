#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

typedef int (*binop)(int, int);

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int div(int a, int b) { return a / b; }

binop ops[4] = { add, sub, mul, div };
const char *names[4] = { "add", "sub", "mul", "div" };

int apply(binop f, int a, int b) { return f(a, b); }

int twice(int (*f)(int), int x) { return f(f(x)); }
int inc(int x) { return x + 1; }
static int counter;
int next(void) { return ++counter; }

int main(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        puts(names[i]);
        show(apply(ops[i], 84, 12));
    }
    show(twice(inc, 5));
    show(next() + next() * 10);
    return 0;
}
