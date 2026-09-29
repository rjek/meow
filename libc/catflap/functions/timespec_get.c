/* timespec_get( struct timespec *, int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>
#include "catflap.h"

int timespec_get( struct timespec * ts, int base )
{
    if ( base != TIME_UTC )
    {
        return 0;
    }
    ts->tv_sec = ( time_t )kernel_time();
    ts->tv_nsec = ( long )( ticks_now() % 100 ) * 10000000L;
    return base;
}
