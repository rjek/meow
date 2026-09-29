/* _PDCLIB_realpath( const char * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdclib/_PDCLIB_glue.h"
#include "catflap.h"

/* An absolute path: the kernel resolves relative ones against the
   working directory, so this only spells that out. */
char * _PDCLIB_realpath( const char * path )
{
    char cwd[128];
    size_t n = strlen( path ), c = 0;
    char * copy;

    if ( path[0] != '/' )
    {
        if ( vfs_getcwd( cwd, sizeof cwd ) < 0 )
        {
            return NULL;
        }
        c = strlen( cwd );
    }
    copy = malloc( c + n + 2 );
    if ( copy == NULL )
    {
        return NULL;
    }
    if ( c > 0 )
    {
        memcpy( copy, cwd, c );
        if ( cwd[c - 1] != '/' )
        {
            copy[c++] = '/';
        }
    }
    memcpy( copy + c, path, n + 1 );
    return copy;
}
