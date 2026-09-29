/* _PDCLIB_close( _PDCLIB_fd_t )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include "pdclib/_PDCLIB_glue.h"

int _PDCLIB_close( _PDCLIB_fd_t fd )
{
    ( void )fd;
    return 0;
}
