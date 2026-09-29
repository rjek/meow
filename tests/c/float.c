#include "msim.h"

/* results go out as bit patterns, so rounding must match the host exactly */
static void show(int n) { print_int(n); putchar('\n'); }
/* a NaN's sign and payload are not specified, so all NaNs print alike */
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

float fa[8] = { 1.0f, -2.5f, 3.0e-3f, 1.0e10f, 7.0f, 0.1f, 0.0f, -0.0f };
double da[8] = { 1.0, -2.5, 3.0e-3, 1.0e10, 7.0, 0.1, 0.0, -0.0 };
int ia[6] = { 0, 1, -1, 123456789, -2147483647, 1000000 };

double avg(double *v, int n)
{
    double s = 0.0;
    int i;
    for (i = 0; i < n; i++) s += v[i];
    return s / n;
}

float root(float x)
{
    float r = x, last = 0.0f;
    int i;
    for (i = 0; i < 30 && r != last; i++) { last = r; r = (r + x / r) * 0.5f; }
    return r;
}

/* the compiler folds constants, and cannot make denormals, so the awkward
 * values are built at run time */
static float fbits(unsigned u) { union { float f; unsigned u; } v; v.u = u; return v.f; }
static double dbits(unsigned hi, unsigned lo) { union { double d; unsigned w[2]; } v; v.w[0] = lo; v.w[1] = hi; return v.d; }

/* globals, so the compiler cannot fold the awkward arithmetic itself */
double big = 1e300, tiny = 1e-300, mind, big18 = 9.2e18;
float bigf = 3.4e38f, minf;

int main(void)
{
    int i, j;

    mind = dbits(0, 1);
    minf = fbits(1);

    fa[6] = fbits(0x000116c2);          /* about 1e-40, a denormal */
    da[6] = dbits(0x00000b8f, 0xd63d6c73);  /* about 1e-310, a denormal */

    for (i = 0; i < 8; i++) for (j = 0; j < 8; j++) {
        showf(fa[i] + fa[j]); showf(fa[i] - fa[j]); showf(fa[i] * fa[j]); showf(fa[i] / fa[j]);
        show((fa[i] < fa[j]) + (fa[i] <= fa[j]) * 2 + (fa[i] == fa[j]) * 4 + (fa[i] >= fa[j]) * 8 + (fa[i] > fa[j]) * 16);
    }
    for (i = 0; i < 8; i++) for (j = 0; j < 8; j++) {
        showd(da[i] + da[j]); showd(da[i] - da[j]); showd(da[i] * da[j]); showd(da[i] / da[j]);
        show((da[i] < da[j]) + (da[i] <= da[j]) * 2 + (da[i] == da[j]) * 4 + (da[i] >= da[j]) * 8 + (da[i] > da[j]) * 16);
    }
    for (i = 0; i < 6; i++) {
        showf((float)ia[i]); showd((double)ia[i]);
        showf((float)(unsigned)ia[i]); showd((double)(unsigned)ia[i]);
        showd((double)(long long)ia[i] * 1000000000LL); showf((float)((unsigned long long)(unsigned)ia[i] << 20));
    }
    /* conversions out of range are undefined in C, and hosts differ, so
     * the operands are kept in range */
    for (i = 0; i < 6; i++) {
        int small = fa[i] > -1e5f && fa[i] < 1e5f;
        show(small ? (int)fa[i] : 0); show(small ? (int)da[i] : 0);
        show(small ? (int)(fa[i] * 12345.678f) : 0); show(small ? (int)(da[i] * 12345.678) : 0);
        show(da[i] > 0 && da[i] < 1.4 ? (int)(unsigned)(da[i] * 3e9) : 0); showd(da[i] > 0 && da[i] < 1.4 ? (double)(unsigned)(da[i] * 3e9) : 0.0);
        showd(small ? (double)(long long)(da[i] * 1e15) : 0.0); showd(da[i] > 0 && da[i] <= 1.0 ? (double)(unsigned long long)(da[i] * 1.5e19) : 0.0);
        showf(-fa[i]); showd(-da[i]); showd((double)fa[i]); showf((float)da[i]);
    }
    show((int)-2147483648.0); show((int)2147483647.0); show((int)-2147483647.0f);
    show((int)-0.99); show((int)0.99); show((int)-1.5f); show((int)(unsigned)4294967295.0);
    showd((double)(long long)-big18); showd((double)(long long)big18); showd((double)(unsigned long long)(big18 * 2.0));
    showd(avg(da, 8));
    showf(root(2.0f)); showf(root(1.0e10f)); showf(root(3.0e-3f));
    showd(da[0] / 3.0); showd(big * 1e10); showd(-tiny * 1e-100); showf(fa[0] / 3.0f);
    showd(da[5] + 0.2); showd(mind * 0.5); showd(mind * 3.0); showd(mind / 3.0); showd(mind * 1.5);
    showf(minf * 0.5f); showf(minf * 3.0f); showf(bigf * 2.0f); showf(bigf + bigf); showf(minf * 2.5f);
    showd(big * big - big * big); showd(-(big * big)); showd((big * big) / (big * big));
    return 0;
}
