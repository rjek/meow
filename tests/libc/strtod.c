#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* strtod and its relations must give the nearest value, which the bits
   show whatever printf makes of it, and stop where the number does; and
   %a must print those bits back */
static const char *const numbers[] = {
    "0", "-0", "1", "0.1", "0.3", "123.456", "1e22", "1e23", "8.41e21", "1e100", "6.02214076e23",
    "5e-324", "2.4703282292062328e-324", "2.4703282292062327e-324", "2.2250738585072014e-308",
    "2.2250738585072011e-308", "1.7976931348623157e308", "1.7976931348623159e308", "1e400", "-1e400",
    "1e-400", "9007199254740993", "9007199254740993.0000000000000000000000000001",
    "123456789012345678901234567890", "0.000000000000000000000000000000000000001e40",
    "1.00000000000000011102230246251565404236316680908203125",
    "1.00000000000000011102230246251565404236316680908203124",
    "1.00000000000000011102230246251565404236316680908203126",
    "  +4.5e1xyz", ".5", "5.", "-.5e-1", "1e", "1e+", "1.e1", ".", "+", "-", "", "e5", "0e0", "1e5000000000",
    "0x10", "0x1p4", "0x.8", "0x1.8p1", "-0xA.8p-2", "0x1.fffffffffffffp1023", "0x1p1024", "0x1p-1074",
    "0x1p-1075", "0x1.00000000000008p0", "0x1.000000000000081p0", "0x1.00000000000018p0", "0x1.00000000000011p0",
    "0x1.0000000000001800000000001p0", "0x1.00000000000027ffffp0", "0x3.0000000000001p0", "0x", "0x.", "0xg",
    "0x1p", "0X1P+3", "inf", "-Infinity", "infinit", "nan", "NAN(abc)", "nan(", "nanx"
};

static void bits(double d)
{
    unsigned long long u;
    memcpy(&u, &d, sizeof u);
    printf("%016llx", u);
}

int main(void)
{
    static const double values[] = { 0.0, -0.0, 1.0, 1.5, 0.1, 255.0, -2.75, 1e-310, 5e-324, 1.7976931348623157e308,
                                     0.999999999, 1.999999999, 1.0 / 3.0, 1024.0 };
    unsigned i;

    for (i = 0; i < sizeof numbers / sizeof numbers[0]; i++) {
        const char *s = numbers[i];
        char *end;
        double d = strtod(s, &end);
        float f = strtof(s, NULL);
        unsigned fu;
        memcpy(&fu, &f, sizeof fu);
        if (d != d) printf("\"%s\": nan", s);
        else { printf("\"%s\": ", s); bits(d); }
        if (f != f) printf(" nan");
        else printf(" %08x", fu);
        printf(" %d\n", (int)(end - s));
    }
    errno = 0; strtod("1e400", NULL); printf("overflow %d", errno == ERANGE);
    errno = 0; strtod("1e-400", NULL); printf(" underflow %d", errno == ERANGE);
    errno = 0; strtod("1e300", NULL); printf(" neither %d", errno == ERANGE);
    printf(" atof %g %g strtold %g\n", atof("-12.5e1"), atof("junk"), (double)strtold("0x1.8p1", NULL));

    for (i = 0; i < sizeof values / sizeof values[0]; i++) {
        double v = values[i];
        printf("%a %A %.0a %.1a %.3a %.13a %.16a %#a %#.0a|%12.2a|%-12.2a|%+012.2a|% a\n",
            v, v, v, v, v, v, v, v, v, v, v, v, v);
    }
    printf("%a %.3A\n", 1e400, -1e400);
    return 0;
}
