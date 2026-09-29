#include "msim.h"

/* constants the front end folds itself: infinities, NaNs, denormals and
 * 64-bit conversions must come out as IEEE arithmetic would make them */
static void show(int n) { print_int(n); putchar('\n'); }
static void showf(float f)
{
    union { float f; unsigned u; } v;
    v.f = f;
    if ((v.u & 0x7f800000) == 0x7f800000 && (v.u & 0x7fffff) != 0) puts("nan");
    else { print_hex(v.u); putchar('\n'); }
}
static void showd(double d)
{
    union { double d; unsigned w[2]; } v;
    v.d = d;
    if ((v.w[1] & 0x7ff00000) == 0x7ff00000 && ((v.w[1] & 0xfffff) != 0 || v.w[0] != 0)) puts("nan");
    else { print_hex(v.w[1]); putchar(':'); print_hex(v.w[0]); putchar('\n'); }
}

double inf = 1e300 * 1e300, ninf = -1.0 / 0.0, nan = 0.0 / 0.0;
double dn1 = 4.9e-324, dn2 = 1e-320 * 0.5, dn3 = 2.2250738585072014e-308 / 4.0;
float fn1 = 1e-40f, fn2 = 1e-45f, fbig = 3.4e38f * 2.0f;
double big = 1e400, tiny = 1e-400;
long long l1 = (long long)9.2e18, l2 = (long long)-9.2e18, l3 = (long long)-1.5;
unsigned long long l4 = (unsigned long long)1.8e19;
double m1 = (double)9000000000LL, m2 = (double)-9000000000LL, m3 = (double)18000000000000000000ULL;
double m4 = (double)(1LL << 53) + 1.0, m5 = (double)((1LL << 53) + 1);
int c1 = (0.0 / 0.0) == (0.0 / 0.0), c2 = (0.0 / 0.0) != 1.0, c3 = (0.0 / 0.0) < 1.0;
int c4 = (0.0 / 0.0) >= 1.0, c5 = 1e300 * 1e300 > 1.0, c6 = -1e300 * 1e300 < -1e308;
int c7 = 4.9e-324 > 0.0, c8 = 4.9e-324 * 0.5 == 0.0, c9 = 1e-320 == 1e-320 * 2.0 / 2.0;

int main(void)
{
    showd(inf); showd(ninf); showd(nan); showd(inf - inf); showd(inf * 0.0); showd(-nan);
    showd(dn1); showd(dn2); showd(dn3); showd(dn1 * 3.0); showd(dn1 + dn1);
    showf(fn1); showf(fn2); showf(fbig); showf(-fbig); showf(fn1 * 0.5f);
    showd(big); showd(tiny); showd(-big); showd(big / big);
    showd((double)l1); showd((double)l2); show((int)l3); showd((double)l4);
    showd(m1); showd(m2); showd(m3); showd(m4); showd(m5);
    show(c1 + c2 * 2 + c3 * 4 + c4 * 8 + c5 * 16 + c6 * 32 + c7 * 64 + c8 * 128 + c9 * 256);
    show((int)(float)16777217.0); show((int)(3.0 * 0.5)); show((int)-2.5);
    show((int)(unsigned)3000000000.0 == (int)0xb2d05e00);
    return 0;
}
