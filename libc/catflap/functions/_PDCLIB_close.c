/* _PDCLIB_close( _PDCLIB_fd_t )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <errno.h>
#include "pdclib/_PDCLIB_glue.h"
#include "catflap.h"

int _PDCLIB_close( _PDCLIB_fd_t fd )
{
    int rc = vfs_close( fd );

    if ( rc < 0 )
    {
        *_PDCLIB_errno_func() = -rc;
        return -1;
    }
    return 0;
}
