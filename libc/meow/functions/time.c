/* time( time_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>

/* Nothing keeps time. */
time_t time( time_t * timer )
{
    if ( timer != NULL )
    {
        *timer = ( time_t )-1;
    }
    return ( time_t )-1;
}
