/* _PDCLIB_rename( const char *, const char * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"

int _PDCLIB_rename( const char * oldpath, const char * newpath )
{
    ( void )oldpath;
    ( void )newpath;
    *_PDCLIB_errno_func() = _PDCLIB_ENOENT;
    return -1;
}
