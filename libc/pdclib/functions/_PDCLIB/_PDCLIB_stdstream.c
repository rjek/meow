/* _PDCLIB_stdstream( int )

   Added for MEOW: the standard streams by number.  Library code is
   compiled so that its data is per process; a program's own code is
   not, so it must ask.
*/
#include <stdio.h>

struct _PDCLIB_file_t * _PDCLIB_stdstream( int which )
{
    switch ( which )
    {
        case 0: return &_PDCLIB_sin;
        case 1: return &_PDCLIB_sout;
        default: return &_PDCLIB_serr;
    }
}
