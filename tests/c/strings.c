#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

int length(const char *s) { int n = 0; while (*s++) n++; return n; }
void copy(char *d, const char *s) { while ((*d++ = *s++) != 0) ; }
int compare(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
void reverse(char *s)
{
    int i = 0, j = length(s) - 1;
    while (i < j) { char t = s[i]; s[i] = s[j]; s[j] = t; i++; j--; }
}
void upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 'a' - 'A'; }

signed char sc[3] = { -5, 100, -128 };
short sh[3] = { -300, 32000, -32768 };
unsigned short us[2] = { 65535, 1 };

int main(void)
{
    char buf[32];
    int i;

    copy(buf, "hello, world");
    show(length(buf));
    puts(buf);
    reverse(buf);
    puts(buf);
    upper(buf);
    puts(buf);
    show(compare("abc", "abd"));
    show(compare("abc", "abc"));
    show(compare("b", "a"));
    for (i = 0; i < 3; i++) show(sc[i]);
    for (i = 0; i < 3; i++) show(sh[i]);
    show(us[0] + us[1]);
    show((sc[0] * sh[0]) / 4);
    return 0;
}
