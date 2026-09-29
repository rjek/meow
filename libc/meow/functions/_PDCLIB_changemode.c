/* _PDCLIB_changemode( struct _PDCLIB_file_t *, unsigned int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <limits.h>
#include <stddef.h>
#include "pdclib/_PDCLIB_glue.h"

/* Nothing can be reopened, so freopen() always fails. */
int _PDCLIB_changemode( struct _PDCLIB_file_t * stream, unsigned int mode )
{
    ( void )stream;
    ( void )mode;
    return INT_MIN;
}
