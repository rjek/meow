/* remove( const char * ): PDCLib's own wants a POSIX unlink. */
#include <stdio.h>
#include "pdclib/_PDCLIB_glue.h"

int remove( const char * pathname )
{
    return _PDCLIB_remove( pathname );
}
