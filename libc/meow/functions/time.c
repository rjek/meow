/* time( time_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>

/* msim hands out the host's clock */
extern long _PDCLIB_meow_time( void );

time_t time( time_t * timer )
{
    time_t t = ( time_t )_PDCLIB_meow_time();

    if ( timer != NULL )
    {
        *timer = t;
    }
    return t;
}
