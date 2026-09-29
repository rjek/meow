/* _PDCLIB_seek( struct _PDCLIB_file_t *, _PDCLIB_int_least64_t, int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"

/* The console cannot seek. */
_PDCLIB_int_least64_t _PDCLIB_seek( struct _PDCLIB_file_t * stream, _PDCLIB_int_least64_t offset, int whence )
{
    ( void )stream;
    ( void )offset;
    ( void )whence;
    *_PDCLIB_errno_func() = _PDCLIB_ESPIPE;
    return EOF;
}
