/* gmtime, localtime and mktime for a machine whose clock is UTC and
   which has no zone files: local time is UTC, without the 12 KB of
   zoneinfo parsing PDCLib carries for the day it might not be.  The
   calendar arithmetic is Howard Hinnant's, valid for every year the
   type can hold.  Used by both builds of the library. */
#include <time.h>
#include <stddef.h>
#include <stdlib.h>
#include "pdclib/_PDCLIB_internal.h"

static struct tm result_tm;

static long long days_from_civil( long long y, int m, int d )
{
    long long era, yoe, doy, doe;

    y -= m <= 2;
    era = ( y >= 0 ? y : y - 399 ) / 400;
    yoe = y - era * 400;
    doy = ( 153 * ( m + ( m > 2 ? -3 : 9 ) ) + 2 ) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days( long long z, long long * y, int * m, int * d )
{
    long long era, doe, yoe, doy, mp;

    z += 719468;
    era = ( z >= 0 ? z : z - 146096 ) / 146097;
    doe = z - era * 146097;
    yoe = ( doe - doe / 1460 + doe / 36524 - doe / 146096 ) / 365;
    *y = yoe + era * 400;
    doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 );
    mp = ( 5 * doy + 2 ) / 153;
    *d = ( int )( doy - ( 153 * mp + 2 ) / 5 + 1 );
    *m = ( int )( mp + ( mp < 10 ? 3 : -9 ) );
    *y += *m <= 2;
}

static struct tm * fill( long long t, struct tm * tm )
{
    long long days = t / 86400, secs = t % 86400, y;
    int m, d;

    if ( secs < 0 )
    {
        secs += 86400;
        days--;
    }
    civil_from_days( days, &y, &m, &d );
    tm->tm_sec = ( int )( secs % 60 );
    tm->tm_min = ( int )( secs / 60 % 60 );
    tm->tm_hour = ( int )( secs / 3600 );
    tm->tm_mday = d;
    tm->tm_mon = m - 1;
    tm->tm_year = ( int )( y - 1900 );
    tm->tm_wday = ( int )( ( days % 7 + 11 ) % 7 );       /* day 0 was a Thursday */
    tm->tm_yday = ( int )( days - days_from_civil( y, 1, 1 ) );
    tm->tm_isdst = 0;
    return tm;
}

struct tm * gmtime( const time_t * timer )
{
    return fill( *timer, &result_tm );
}

struct tm * localtime( const time_t * timer )
{
    return fill( *timer, &result_tm );
}

struct tm * gmtime_s( const time_t * _PDCLIB_restrict timer, struct tm * _PDCLIB_restrict result )
{
    if ( timer == NULL || result == NULL )
    {
        _PDCLIB_constraint_handler( _PDCLIB_CONSTRAINT_VIOLATION( _PDCLIB_EINVAL ) );
        return NULL;
    }
    return fill( *timer, result );
}

struct tm * localtime_s( const time_t * _PDCLIB_restrict timer, struct tm * _PDCLIB_restrict result )
{
    return gmtime_s( timer, result );
}

/* The fields need not be in range: months carry into years, and so on,
   as the standard says; the struct comes back normalised. */
time_t mktime( struct tm * tm )
{
    long long y = ( long long )tm->tm_year + 1900 + tm->tm_mon / 12;
    int m = tm->tm_mon % 12;
    long long t;

    if ( m < 0 )
    {
        m += 12;
        y--;
    }
    t = days_from_civil( y, m + 1, 1 ) + tm->tm_mday - 1;
    t = t * 86400 + ( long long )tm->tm_hour * 3600 + ( long long )tm->tm_min * 60 + tm->tm_sec;
    if ( ( time_t )t != t )
    {
        return ( time_t )-1;
    }
    fill( t, tm );
    return ( time_t )t;
}
