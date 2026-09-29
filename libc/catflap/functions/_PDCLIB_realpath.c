/* _PDCLIB_realpath( const char * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdclib/_PDCLIB_glue.h"

/* The kernel's paths are absolute already; a relative one is taken from
   the root until there is a working directory to take it from. */
char * _PDCLIB_realpath( const char * path )
{
    size_t n = strlen( path );
    char * copy = malloc( n + 2 );

    if ( copy == NULL )
    {
        return NULL;
    }
    if ( path[0] == '/' )
    {
        memcpy( copy, path, n + 1 );
    }
    else
    {
        copy[0] = '/';
        memcpy( copy + 1, path, n + 1 );
    }
    return copy;
}
