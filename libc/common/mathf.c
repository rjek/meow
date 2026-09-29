/* The float functions of <math.h>, as the double ones rounded.  On a
   machine whose float and double arithmetic are both software this is
   no slower than a separate float implementation, is correctly rounded
   wherever the double function is, and is a hundred functions shorter.
   fmaf, nextafterf and nexttowardf are not here: they cannot be had
   from the double function, and musl's stay.  Used by both builds of
   the library. */
#include <math.h>

#define WRAP1(n) float n##f( float x ) { return ( float )n( x ); }
#define WRAP2(n) float n##f( float x, float y ) { return ( float )n( x, y ); }

WRAP1( acos ) WRAP1( asin ) WRAP1( atan ) WRAP2( atan2 )
WRAP1( cos ) WRAP1( sin ) WRAP1( tan )
WRAP1( acosh ) WRAP1( asinh ) WRAP1( atanh )
WRAP1( cosh ) WRAP1( sinh ) WRAP1( tanh )
WRAP1( exp ) WRAP1( exp2 ) WRAP1( expm1 )
WRAP1( log ) WRAP1( log10 ) WRAP1( log1p ) WRAP1( log2 ) WRAP1( logb )
WRAP1( cbrt ) WRAP2( hypot ) WRAP2( pow ) WRAP1( sqrt )
WRAP1( erf ) WRAP1( erfc ) WRAP1( lgamma ) WRAP1( tgamma )
WRAP1( ceil ) WRAP1( floor ) WRAP1( nearbyint ) WRAP1( rint ) WRAP1( round ) WRAP1( trunc )
WRAP2( fmod ) WRAP2( remainder ) WRAP2( copysign )

int ilogbf( float x ) { return ilogb( x ); }
long lrintf( float x ) { return lrint( x ); }
long long llrintf( float x ) { return llrint( x ); }
long lroundf( float x ) { return lround( x ); }
long long llroundf( float x ) { return llround( x ); }
float ldexpf( float x, int n ) { return ( float )ldexp( x, n ); }
float scalbnf( float x, int n ) { return ( float )scalbn( x, n ); }
float scalblnf( float x, long n ) { return ( float )scalbln( x, n ); }

float frexpf( float x, int * e )
{
    return ( float )frexp( x, e );
}

float modff( float x, float * iptr )
{
    double i;
    float r = ( float )modf( x, &i );

    *iptr = ( float )i;
    return r;
}

float remquof( float x, float y, int * quo )
{
    return ( float )remquo( x, y, quo );
}

float lgammaf_r( float x, int * signgamp )
{
    return ( float )lgamma_r( x, signgamp );
}
