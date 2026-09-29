/* _PDCLIB_fillbuffer( struct _PDCLIB_file_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"

extern int _PDCLIB_meow_getc( void );

/* Standard input is the console, read a line at a time as a terminal
   would give it: up to and including a newline, or to the end of input.
   Nothing else can be read. */
int _PDCLIB_fillbuffer( struct _PDCLIB_file_t * stream )
{
    _PDCLIB_size_t n = 0;

    if ( stream->handle != 0 )
    {
        *_PDCLIB_errno_func() = _PDCLIB_EBADF;
        stream->status |= _PDCLIB_ERRORFLAG;
        return EOF;
    }
    while ( n < stream->bufsize )
    {
        int c = _PDCLIB_meow_getc();

        if ( c == -1 )
        {
            break;
        }
        stream->buffer[ n++ ] = ( char )c;
        if ( c == '\n' )
        {
            break;
        }
    }
    if ( n == 0 )
    {
        stream->status |= _PDCLIB_EOFFLAG;
        return EOF;
    }
    stream->pos.offset += n;
    stream->bufend = n;
    stream->bufidx = 0;
    return 0;
}
