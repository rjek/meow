/* clock( void )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>

/* Nothing keeps time. */
clock_t clock( void )
{
    return ( clock_t )-1;
}
