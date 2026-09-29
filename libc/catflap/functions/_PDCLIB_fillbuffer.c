/* _PDCLIB_fillbuffer( struct _PDCLIB_file_t * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"
#include "catflap.h"

int _PDCLIB_fillbuffer( struct _PDCLIB_file_t * stream )
{
    int n = vfs_read( stream->handle, stream->buffer, stream->bufsize );

    if ( n < 0 )
    {
        *_PDCLIB_errno_func() = -n;
        stream->status |= _PDCLIB_ERRORFLAG;
        return EOF;
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
