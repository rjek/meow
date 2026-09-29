/* getenv_s( size_t *, char *, rsize_t, const char * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#define __STDC_WANT_LIB_EXT1__ 1
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

/* There is no environment: every name is unset. */
errno_t getenv_s( size_t * _PDCLIB_restrict len, char * _PDCLIB_restrict value, rsize_t maxsize, const char * _PDCLIB_restrict name )
{
    if ( name == NULL || maxsize == 0 || maxsize > RSIZE_MAX || value == NULL )
    {
        _PDCLIB_constraint_handler( _PDCLIB_CONSTRAINT_VIOLATION( _PDCLIB_EINVAL ) );
        return _PDCLIB_EINVAL;
    }
    if ( len != NULL )
    {
        *len = 0;
    }
    value[ 0 ] = '\0';
    return -1;
}
