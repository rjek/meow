/* malloc, calloc, realloc and free: the allocator of Kernighan and
   Ritchie's "The C Programming Language", section 8.7.  Free blocks sit
   on a circular list in address order, allocation takes the first that
   fits, and a freed block merges with its neighbours if they are free.
   The heap grows through sbrk(), whose pieces need not adjoin.  Nothing
   is ever given back to sbrk, and nothing is locked: one thread at a
   time.  Chosen for being small, not for speed or fragmentation, and
   used in place of PDCLib's dlmalloc by both builds of the library. */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

extern void * sbrk( intptr_t increment );

/* a block's header; its size counts headers, the header's own included */
typedef union header
{
    struct
    {
        union header * next;            /* on the free list */
        size_t size;
    } s;
    double align;                       /* what malloc's result must suit */
} header;

#define GROW 1024                       /* the fewest headers to ask sbrk for */

static header base;                     /* an empty list's only member */
static header * freep;                  /* where the last search stopped */

static header * more( size_t units )
{
    header * h;
    void * p;

    if ( units < GROW )
    {
        units = GROW;
    }
    if ( units > ( SIZE_MAX / sizeof( header ) ) )
    {
        return NULL;
    }
    p = sbrk( ( intptr_t )( units * sizeof( header ) ) );
    if ( p == ( void * )-1 )
    {
        return NULL;
    }
    h = p;
    h->s.size = units;
    free( h + 1 );
    return freep;
}

void * malloc( size_t size )
{
    header * p, * prev;
    size_t units;

    if ( size > SIZE_MAX - 2 * sizeof( header ) )
    {
        errno = ENOMEM;
        return NULL;
    }
    units = ( size + sizeof( header ) - 1 ) / sizeof( header ) + 1;
    if ( freep == NULL )
    {
        base.s.next = &base;
        base.s.size = 0;
        freep = &base;
    }
    prev = freep;
    for ( p = prev->s.next; ; prev = p, p = p->s.next )
    {
        if ( p->s.size >= units )
        {
            if ( p->s.size == units )
            {
                prev->s.next = p->s.next;
            }
            else
            {
                /* the tail end of the block, so the list need not change */
                p->s.size -= units;
                p += p->s.size;
                p->s.size = units;
            }
            freep = prev;
            return p + 1;
        }
        if ( p == freep && ( p = more( units ) ) == NULL )
        {
            errno = ENOMEM;
            return NULL;
        }
    }
}

void free( void * ptr )
{
    header * b, * p;

    if ( ptr == NULL )
    {
        return;
    }
    if ( freep == NULL )                /* only more() frees before a malloc */
    {
        base.s.next = &base;
        base.s.size = 0;
        freep = &base;
    }
    b = ( header * )ptr - 1;
    /* find where it goes: between p and the next, or at an end of the list */
    for ( p = freep; ! ( b > p && b < p->s.next ); p = p->s.next )
    {
        if ( p >= p->s.next && ( b > p || b < p->s.next ) )
        {
            break;
        }
    }
    if ( b + b->s.size == p->s.next )
    {
        b->s.size += p->s.next->s.size;
        b->s.next = p->s.next->s.next;
    }
    else
    {
        b->s.next = p->s.next;
    }
    if ( p + p->s.size == b )
    {
        p->s.size += b->s.size;
        p->s.next = b->s.next;
    }
    else
    {
        p->s.next = b;
    }
    freep = p;
}

void * calloc( size_t nmemb, size_t size )
{
    void * p;

    if ( size != 0 && nmemb > SIZE_MAX / size )
    {
        errno = ENOMEM;
        return NULL;
    }
    p = malloc( nmemb * size );
    if ( p != NULL )
    {
        memset( p, 0, nmemb * size );
    }
    return p;
}

/* in place when it already fits, else moved */
void * realloc( void * ptr, size_t size )
{
    header * b;
    size_t have;
    void * p;

    if ( ptr == NULL )
    {
        return malloc( size );
    }
    b = ( header * )ptr - 1;
    have = ( b->s.size - 1 ) * sizeof( header );
    if ( size <= have )
    {
        return ptr;
    }
    p = malloc( size );
    if ( p != NULL )
    {
        memcpy( p, ptr, have );
        free( ptr );
    }
    return p;
}
