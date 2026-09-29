/* clock( void )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>
#include "catflap.h"

/* the kernel's ticks, 100 a second, against CLOCKS_PER_SEC of 1000000 */
clock_t clock( void )
{
    return ( clock_t )ticks_now() * 10000;
}
