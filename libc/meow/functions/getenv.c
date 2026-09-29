/* getenv( const char * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdlib.h>

/* There is no environment. */
char * getenv( const char * name )
{
    ( void )name;
    return NULL;
}
