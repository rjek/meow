/* _PDCLIB_fillbuffer( struct _PDCLIB_file_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include "pdclib/_PDCLIB_glue.h"

/* msim has no console input, so standard input is always at its end. */
int _PDCLIB_fillbuffer( struct _PDCLIB_file_t * stream )
{
    stream->status |= _PDCLIB_EOFFLAG;
    return EOF;
}
