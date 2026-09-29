/* lgamma_r and lgammaf_r: musl declares these as weak aliases of its
   internal versions, which Norcroft cannot express, so they are calls. */
#include <math.h>

double __lgamma_r( double x, int * signgamp );
float __lgammaf_r( float x, int * signgamp );

double lgamma_r( double x, int * signgamp )
{
    return __lgamma_r( x, signgamp );
}

float lgammaf_r( float x, int * signgamp )
{
    return __lgammaf_r( x, signgamp );
}
