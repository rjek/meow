/* sbrk( intptr_t ), for malloc: the process's heap, from the kernel. */
#include <stdint.h>
#include <stddef.h>
#include "catflap.h"

void * sbrk( intptr_t increment )
{
    return process_sbrk( ( int )increment );
}
