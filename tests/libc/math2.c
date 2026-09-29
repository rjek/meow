#include <stdio.h>
#include <math.h>

/* trigonometric and hyperbolic functions, and their inverses */
static const double xs[] = { -3.0, -1.0, -0.5, 0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 100.0 };

int main(void)
{
    unsigned i;

    for (i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        double x = xs[i];
        printf("%g: sin %.6g cos %.6g tan %.6g sinh %.6g cosh %.6g tanh %.6g\n",
            x, sin(x), cos(x), tan(x), sinh(x), cosh(x), tanh(x));
        printf("   atan %.6g atan2 %.6g asinh %.6g", atan(x), atan2(x, -2.0), asinh(x));
        if (fabs(x) <= 1.0) {
            printf(" asin %.6g acos %.6g", asin(x), acos(x));
        }
        if (x >= 1.0) {
            printf(" acosh %.6g", acosh(x));
        }
        if (fabs(x) < 1.0) {
            printf(" atanh %.6g", atanh(x));
        }
        printf("\n");
    }
    printf("float: sinf %.6g cosf %.6g tanf %.6g atan2f %.6g tanhf %.6g\n",
        sinf(1.0f), cosf(1.0f), tanf(1.0f), atan2f(1.0f, 1.0f), tanhf(0.5f));
    printf("pi %.6g e %.6g\n", 4.0 * atan(1.0), exp(1.0));
    return 0;
}
