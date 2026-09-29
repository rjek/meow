/* timespec_get( struct timespec *, int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>

/* whole seconds from msim's host clock */
extern long _PDCLIB_meow_time( void );

int timespec_get( struct timespec * ts, int base )
{
    if ( base != TIME_UTC )
    {
        return 0;
    }
    ts->tv_sec = ( time_t )_PDCLIB_meow_time();
    ts->tv_nsec = 0;
    return base;
}
