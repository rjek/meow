/* _PDCLIB_open( const char *, unsigned int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"
#include "catflap.h"

_PDCLIB_fd_t _PDCLIB_open( const char * const filename, unsigned int mode )
{
    int flags, rc;

    switch ( mode & ( _PDCLIB_FREAD | _PDCLIB_FWRITE | _PDCLIB_FAPPEND | _PDCLIB_FRW ) )
    {
        case _PDCLIB_FREAD:
            flags = CF_O_RDONLY;
            break;
        case _PDCLIB_FWRITE:
            flags = CF_O_WRONLY | CF_O_CREAT | CF_O_TRUNC;
            break;
        case _PDCLIB_FAPPEND:
            flags = CF_O_WRONLY | CF_O_CREAT | CF_O_APPEND;
            break;
        case _PDCLIB_FREAD | _PDCLIB_FRW:
            flags = CF_O_RDWR;
            break;
        case _PDCLIB_FWRITE | _PDCLIB_FRW:
            flags = CF_O_RDWR | CF_O_CREAT | CF_O_TRUNC;
            break;
        case _PDCLIB_FAPPEND | _PDCLIB_FRW:
            flags = CF_O_RDWR | CF_O_CREAT | CF_O_APPEND;
            break;
        default:
            *_PDCLIB_errno_func() = _PDCLIB_EINVAL;
            return _PDCLIB_NOHANDLE;
    }
    rc = vfs_open( filename, flags );
    if ( rc < 0 )
    {
        *_PDCLIB_errno_func() = -rc;
        return _PDCLIB_NOHANDLE;
    }
    return rc;
}
