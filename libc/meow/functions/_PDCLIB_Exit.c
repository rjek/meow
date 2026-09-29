/* _PDCLIB_Exit( int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdlib.h>
#include "pdclib/_PDCLIB_glue.h"

extern void _PDCLIB_meow_halt( int status );

void _PDCLIB_Exit( int status )
{
    _PDCLIB_meow_halt( status );
}
