#include <stdio.h>
#include <math.h>

/* the elementary functions, six significant digits so that a last-place
   difference between musl and the host does not matter */
static const double xs[] = { 0.0, 0.5, 1.0, 2.0, 3.75, 10.0, 123.456, 1e-5, 1e5 };

int main(void)
{
    unsigned i;
    int e;
    double frac, ip;

    for (i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        double x = xs[i];
        printf("%g: sqrt %.6g cbrt %.6g exp %.6g log %.6g log10 %.6g log2 %.6g\n",
            x, sqrt(x), cbrt(x), exp(x), log(x), log10(x), log2(x));
        printf("   pow2 %.6g pow0.5 %.6g hypot %.6g expm1 %.6g log1p %.6g exp2 %.6g\n",
            pow(x, 2.0), pow(x, 0.5), hypot(x, 3.0), expm1(x), log1p(x), exp2(x));
    }
    for (i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        double x = xs[i] - 2.0;
        printf("%g: floor %g ceil %g trunc %g round %g rint %g nearbyint %g\n",
            x, floor(x), ceil(x), trunc(x), round(x), rint(x), nearbyint(x));
        printf("   lround %ld lrint %ld llround %lld fabs %g fmod %g remainder %g\n",
            lround(x), lrint(x), llround(x), fabs(x), fmod(x, 1.5), remainder(x, 1.5));
    }
    frac = frexp(48.0, &e);
    printf("frexp %g %d ldexp %g scalbn %g ilogb %d logb %g\n",
        frac, e, ldexp(0.75, 6), scalbn(1.0, -3), ilogb(1000.0), logb(1000.0));
    frac = modf(-3.625, &ip);
    printf("modf %g %g copysign %g %g fmax %g fmin %g fdim %g %g fma %g\n",
        ip, frac, copysign(2.0, -0.0), copysign(-2.0, 1.0), fmax(-1.0, 2.0),
        fmin(-1.0, 2.0), fdim(5.0, 3.0), fdim(3.0, 5.0), fma(2.0, 3.0, 4.0));
    printf("nextafter %d %d\n", nextafter(1.0, 2.0) > 1.0, nextafter(1.0, 0.0) < 1.0);
    printf("float: sqrtf %.6g expf %.6g logf %.6g powf %.6g floorf %g fmodf %g\n",
        sqrtf(2.0f), expf(1.0f), logf(10.0f), powf(2.0f, 10.0f), floorf(-2.5f), fmodf(7.5f, 2.0f));
    return 0;
}
