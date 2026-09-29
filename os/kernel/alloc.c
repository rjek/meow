/* The kernel heap: one arena, first fit, an implicit list of blocks
   each behind an 8-byte header.  Adjacent free blocks merge as the
   walk passes them, so there is no free list to keep right. */
#include "kernel.h"

#define ALIGN 8
#define MAGIC 0xcaf0u                   /* low 4 bits: 1 if free */

struct block {
    uint32_t size;                      /* whole block, header included */
    uint32_t tag;                       /* MAGIC | free, and the owner later */
};

static struct block *first, *limit;
static size_t total;

void alloc_init(void *base, void *end)
{
    first = (struct block *)(((uintptr_t)base + ALIGN - 1) & ~(uintptr_t)(ALIGN - 1));
    limit = (struct block *)((uintptr_t)end & ~(uintptr_t)(ALIGN - 1));
    first->size = (uint32_t)((char *)limit - (char *)first);
    first->tag = MAGIC | 1;
    total = first->size;
}

static struct block *after(struct block *b)
{
    return (struct block *)((char *)b + b->size);
}

static int is_free(struct block *b)
{
    return (b->tag & 1) != 0;
}

static void merge_forward(struct block *b)
{
    struct block *n = after(b);

    while (n < limit && is_free(n)) {
        b->size += n->size;
        n = after(b);
    }
}

void *kmalloc(size_t n)
{
    struct block *b;
    uint32_t need = (uint32_t)((n + sizeof *b + ALIGN - 1) & ~(size_t)(ALIGN - 1));

    for (b = first; b < limit; b = after(b)) {
        if ((b->tag & ~1u) != MAGIC) {
            kpanic("heap corrupt at %p", (void *)b);
        }
        if (is_free(b) == 0) {
            continue;
        }
        merge_forward(b);
        if (b->size < need) {
            continue;
        }
        if (b->size - need >= sizeof *b + ALIGN) {
            struct block *rest = (struct block *)((char *)b + need);

            rest->size = b->size - need;
            rest->tag = MAGIC | 1;
            b->size = need;
        }
        b->tag = MAGIC;
        return b + 1;
    }
    return NULL;
}

void kfree(void *p)
{
    struct block *b;

    if (p == NULL) {
        return;
    }
    b = (struct block *)p - 1;
    if (b->tag != MAGIC) {
        kpanic("bad free of %p", p);
    }
    b->tag = MAGIC | 1;
}

size_t kmem_free(void)
{
    struct block *b;
    size_t n = 0;

    for (b = first; b < limit; b = after(b)) {
        if (is_free(b)) {
            n += b->size;
        }
    }
    return n;
}
