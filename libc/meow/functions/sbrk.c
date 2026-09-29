/* sbrk( intptr_t ), for dlmalloc.
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <stdint.h>
#include <stddef.h>

/* The heap is whatever lies between the end of the program's data and
   the stack, which crt0 starts at the top of msim's RAM.  The stack is
   allowed the top 8 KB. */
extern char __bss_end[];
#define HEAP_LIMIT ( ( char * )0x0800E000 )

static char * brk_now;

void * sbrk( intptr_t increment )
{
    char * old;

    if ( brk_now == NULL )
    {
        brk_now = ( char * )( ( ( uintptr_t )__bss_end + 7 ) & ~( uintptr_t )7 );
    }
    old = brk_now;
    if ( increment > 0 && ( intptr_t )( HEAP_LIMIT - brk_now ) < increment )
    {
        return ( void * )-1;
    }
    if ( increment < 0 && ( intptr_t )( brk_now - __bss_end ) < -increment )
    {
        return ( void * )-1;
    }
    brk_now += increment;
    return old;
}
