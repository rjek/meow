/* _PDCLIB_flushbuffer( struct _PDCLIB_file_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"

extern void _PDCLIB_meow_putc( int c );

/* Standard output and standard error both go to the console; nothing
   else can be written. */
int _PDCLIB_flushbuffer( struct _PDCLIB_file_t * stream )
{
    _PDCLIB_size_t i;

    if ( stream->handle != 1 && stream->handle != 2 )
    {
        *_PDCLIB_errno_func() = _PDCLIB_EBADF;
        stream->status |= _PDCLIB_ERRORFLAG;
        return EOF;
    }
    for ( i = 0; i < stream->bufidx; ++i )
    {
        _PDCLIB_meow_putc( ( unsigned char )stream->buffer[ i ] );
    }
    stream->pos.offset += stream->bufidx;
    stream->bufidx = 0;
    return 0;
}
