/* clock( void )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>

/* msim's instruction count, so CLOCKS_PER_SEC pretends to 1 MHz */
extern unsigned long _PDCLIB_meow_cycles( void );

clock_t clock( void )
{
    return ( clock_t )_PDCLIB_meow_cycles();
}
