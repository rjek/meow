/* strtod, strtof, strtold and atof: musl's, which are its __floatscan
 * given a string to read.  The result is the nearest value of the type
 * to the number written, in decimal or in C99's hexadecimal, whatever
 * its length; PDCLib's own, which these replace, multiply the digits up
 * in floating point, by its own account "nowhere good enough", and do not
 * get past the first digit of a hexadecimal number. */
#include <stdlib.h>
#include "../meow/musl/shgetc.h"

long double __floatscan( FILE *, int, int );

static long double strtox( const char * s, char ** p, int prec )
{
    FILE f;
    long double y;
    long cnt;

    sh_fromstring( &f, s );
    shlim( &f, 0 );
    y = __floatscan( &f, prec, 1 );
    cnt = shcnt( &f );
    if ( p ) *p = (char *)s + cnt;
    return y;
}

float strtof( const char * restrict s, char ** restrict p )
{
    return strtox( s, p, 0 );
}

double strtod( const char * restrict s, char ** restrict p )
{
    return strtox( s, p, 1 );
}

long double strtold( const char * restrict s, char ** restrict p )
{
    return strtox( s, p, 2 );
}

double atof( const char * s )
{
    return strtod( s, NULL );
}
