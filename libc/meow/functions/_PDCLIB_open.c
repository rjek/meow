/* _PDCLIB_open( const char *, unsigned int )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"

/* There is no filesystem: only the three standard streams exist, and
   they are set up in _PDCLIB_stdinit.c without being opened. */
_PDCLIB_fd_t _PDCLIB_open( const char * const filename, unsigned int mode )
{
    ( void )filename;
    ( void )mode;
    *_PDCLIB_errno_func() = _PDCLIB_ENOENT;
    return _PDCLIB_NOHANDLE;
}
