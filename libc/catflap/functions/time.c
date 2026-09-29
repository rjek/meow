/* time( time_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>
#include "catflap.h"

time_t time( time_t * timer )
{
    time_t t = ( time_t )kernel_time();

    if ( timer != NULL )
    {
        *timer = t;
    }
    return t;
}
