/* _PDCLIB_seek( struct _PDCLIB_file_t *, _PDCLIB_int_least64_t, int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"
#include "catflap.h"

_PDCLIB_int_least64_t _PDCLIB_seek( struct _PDCLIB_file_t * stream,
                                   _PDCLIB_int_least64_t offset, int whence )
{
    int rc;

    switch ( whence )
    {
        case SEEK_SET: whence = CF_SEEK_SET; break;
        case SEEK_CUR: whence = CF_SEEK_CUR; break;
        case SEEK_END: whence = CF_SEEK_END; break;
        default:
            *_PDCLIB_errno_func() = _PDCLIB_EINVAL;
            return EOF;
    }
    rc = vfs_seek( stream->handle, ( int )offset, whence );
    if ( rc < 0 )
    {
        *_PDCLIB_errno_func() = -rc;
        return EOF;
    }
    stream->ungetidx = 0;
    stream->bufidx = 0;
    stream->bufend = 0;
    stream->pos.offset = rc;
    return rc;
}
