/* system( const char * )
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdlib.h>

/* No command processor: system( NULL ) says so, anything else fails. */
int system( const char * string )
{
    ( void )string;
    return string == NULL ? 0 : -1;
}
