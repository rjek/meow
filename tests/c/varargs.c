#include <stdarg.h>
#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

int sum(int n, ...)
{
    va_list ap;
    int total = 0;
    va_start(ap, n);
    while (n-- > 0) total += va_arg(ap, int);
    va_end(ap);
    return total;
}

void fmt(const char *f, ...)
{
    va_list ap;
    va_start(ap, f);
    for (; *f; f++) {
        if (*f != '%') { putchar(*f); continue; }
        f++;
        switch (*f) {
        case 'd': print_int(va_arg(ap, int)); break;
        case 'x': print_hex(va_arg(ap, unsigned)); break;
        case 's': puts(va_arg(ap, const char *)); break;
        case 'c': putchar(va_arg(ap, int)); break;
        default: putchar('?'); break;
        }
    }
    va_end(ap);
}

int main(void)
{
    show(sum(0));
    show(sum(1, 5));
    show(sum(4, 1, 2, 3, 4));
    show(sum(8, 1, 2, 3, 4, 5, 6, 7, 8));
    fmt("d=%d x=%x c=%c s=%s", -42, 0xcafe, 'Z', "str");
    fmt("%d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6);
    return 0;
}
