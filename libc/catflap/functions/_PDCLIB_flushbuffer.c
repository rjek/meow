/* _PDCLIB_flushbuffer( struct _PDCLIB_file_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"
#include "catflap.h"

int _PDCLIB_flushbuffer( struct _PDCLIB_file_t * stream )
{
    _PDCLIB_size_t written = 0;

    while ( written < stream->bufidx )
    {
        int n = vfs_write( stream->handle, stream->buffer + written,
                           stream->bufidx - written );

        if ( n <= 0 )
        {
            *_PDCLIB_errno_func() = n < 0 ? -n : _PDCLIB_EIO;
            stream->status |= _PDCLIB_ERRORFLAG;
            /* keep what was not written */
            memmove( stream->buffer, stream->buffer + written, stream->bufidx - written );
            stream->bufidx -= written;
            return EOF;
        }
        written += ( _PDCLIB_size_t )n;
    }
    stream->pos.offset += written;
    stream->bufidx = 0;
    return 0;
}
