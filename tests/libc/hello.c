#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

/* the C library from the outside: stdio, the heap, conversions, maths */
static int cmp(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

int main(void)
{
    int *v;
    int i;
    char buf[64];
    char *end;
    double d;

    printf("hello, world\n");
    printf("%d %5d %-5d| %x %o %u %c %s %%\n", 42, -7, 7, 255, 8, 3000000000u, 'z', "str");
    printf("%ld %lld %llu\n", -2147483647L - 1, -9223372036854775807LL - 1, 18446744073709551615ULL);
    printf("%f %e %g %.3f %10.2f|\n", 3.14159265358979, 12345.678, 0.0001234, -2.0 / 3.0, 1e6);

    v = malloc(20 * sizeof *v);
    for (i = 0; i < 20; i++) v[i] = (i * 7919) % 101;
    qsort(v, 20, sizeof *v, cmp);
    for (i = 0; i < 20; i++) printf("%d ", v[i]);
    putchar('\n');
    v = realloc(v, 1000 * sizeof *v);
    for (i = 0; i < 1000; i++) v[i] = i;
    printf("%d %d\n", v[999], (int)strlen("twelve chars"));
    free(v);

    d = strtod("  -12.5e2xyz", &end);
    printf("%g [%s] %ld %d\n", d, end, strtol("0x7fffffff", NULL, 0), atoi("-99"));
    sprintf(buf, "%08.3f|%+d|%5s|%-5s|", 3.14159, 5, "ab", "cd");
    puts(buf);
    snprintf(buf, sizeof buf, "%s", "abcdefghijklmnopqrstuvwxyz");
    printf("%s %d\n", buf, (int)strlen(buf));
    for (i = 0; buf[i] != '\0'; i++) buf[i] = (char)toupper((unsigned char)buf[i]);
    printf("%s %s\n", buf, strchr(buf, 'M'));
    printf("%d %d %d\n", memcmp("abc", "abd", 3) < 0, strncmp("hello", "help", 3), (int)strspn("aabbcc", "ab"));

    printf("%.6f %.6f %.6f %.6f\n", sin(1.0), cos(1.0), exp(1.0), log(10.0));
    printf("%.6f %.6f %.6f %.6f\n", sqrt(2.0), pow(2.0, 10.5), atan2(1.0, 2.0), fabs(-3.5));
    printf("%.6f %.6f %d %d\n", floor(-2.5), ceil(-2.5), (int)lrint(2.5), (int)lrint(3.5));
    printf("%d %d %d\n", isnan(NAN), isinf(INFINITY), isfinite(1.0));
    printf("%.4f %.4f %.4f\n", tan(0.5), asin(0.5), log2(1024.0));
    return 3;
}
