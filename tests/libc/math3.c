#include <stdio.h>
#include <math.h>
#include <float.h>

/* special values, classification, and the gamma and error functions */
static const char *kind(double x)
{
    switch (fpclassify(x)) {
    case FP_NAN: return "nan";
    case FP_INFINITE: return "inf";
    case FP_ZERO: return "zero";
    case FP_SUBNORMAL: return "subnormal";
    case FP_NORMAL: return "normal";
    }
    return "?";
}

static void show(const char *what, double x)
{
    /* not the sign of a NaN: the host may well give a negative one */
    printf("%s: %s sign %d finite %d\n", what, kind(x), isnan(x) == 0 && signbit(x) != 0, isfinite(x) != 0);
}

int main(void)
{
    double x;

    show("sqrt(-1)", sqrt(-1.0));
    show("log(0)", log(0.0));
    show("log(-1)", log(-1.0));
    show("exp(1000)", exp(1000.0));
    show("exp(-1000)", exp(-1000.0));
    show("-0.0", -0.0);
    show("DBL_MIN/2", DBL_MIN / 2.0);
    show("DBL_MAX*2", DBL_MAX * 2.0);
    show("HUGE_VAL", HUGE_VAL);
    show("-INFINITY", -INFINITY);
    show("NAN", NAN);
    show("atan(inf)", atan(INFINITY));
    show("pow(0,0)", pow(0.0, 0.0));
    show("pow(2,inf)", pow(2.0, INFINITY));
    show("pow(-8,1/3)", pow(-8.0, 1.0 / 3.0));
    printf("pow(0,0) %g pow(-2,3) %g pow(-2,0.5) nan %d\n", pow(0.0, 0.0), pow(-2.0, 3.0), isnan(pow(-2.0, 0.5)) != 0);
    printf("atan(inf) %.6g tanh(inf) %g fmod(1,0) nan %d fmod(inf,1) nan %d\n",
        atan(INFINITY), tanh(INFINITY), isnan(fmod(1.0, 0.0)) != 0, isnan(fmod(INFINITY, 1.0)) != 0);
    printf("nan==nan %d isnan %d isinf %d isnormal %d isunordered %d\n",
        NAN == NAN, isnan(NAN) != 0, isinf(-INFINITY) != 0, isnormal(1.0) != 0, isunordered(NAN, 1.0) != 0);
    printf("isgreater %d isless %d islessequal %d\n",
        isgreater(2.0, 1.0) != 0, isless(NAN, 1.0) != 0, islessequal(1.0, 1.0) != 0);
    for (x = 0.5; x <= 5.0; x += 0.75) {
        printf("%g: tgamma %.6g lgamma %.6g erf %.6g erfc %.6g\n",
            x, tgamma(x), lgamma(x), erf(x), erfc(x));
    }
    printf("erf(-1) %.6g erfc(10) %.6g lgamma(100) %.6g tgamma(0.5)^2 %.6g\n",
        erf(-1.0), erfc(10.0), lgamma(100.0), tgamma(0.5) * tgamma(0.5));
    printf("DBL_EPSILON %g DBL_MANT_DIG %d DBL_MAX_EXP %d FLT_EPSILON %g\n",
        DBL_EPSILON, DBL_MANT_DIG, DBL_MAX_EXP, FLT_EPSILON);
    return 0;
}
