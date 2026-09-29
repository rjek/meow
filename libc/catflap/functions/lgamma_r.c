/* lgamma_r: musl declares it as a weak alias of its internal version,
   which Norcroft cannot express, so it is a call.  lgammaf_r is with
   the other float wrappers in libc/common/mathf.c. */
#include <math.h>

double __lgamma_r( double x, int * signgamp );

double lgamma_r( double x, int * signgamp )
{
    return __lgamma_r( x, signgamp );
}
