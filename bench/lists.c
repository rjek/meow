#include "bench.h"

/* structures and pointers: a sorted linked list built, walked and freed
 * from a static pool, and structs passed and returned by value */
struct node { int key; short weight; unsigned char tag; struct node *next; };
struct pair { int lo, hi; };

static struct node pool[400];
static int used;

static struct node *alloc(int key)
{
    struct node *n = &pool[used++];
    n->key = key; n->weight = (short)(key * 3); n->tag = (unsigned char)key; n->next = 0;
    return n;
}

static struct node *insert(struct node *head, struct node *n)
{
    struct node **pp = &head;
    while (*pp && (*pp)->key < n->key) pp = &(*pp)->next;
    n->next = *pp; *pp = n;
    return head;
}

static struct pair minmax(const struct node *head)
{
    struct pair p;
    p.lo = head->key; p.hi = head->key;
    for (; head; head = head->next) { if (head->key < p.lo) p.lo = head->key; if (head->key > p.hi) p.hi = head->key; }
    return p;
}

static int span(struct pair p) { return p.hi - p.lo; }

int main(void)
{
    unsigned sum = 0, seed = 7;
    int iter, i;

    for (iter = 0; iter < 8; iter++) {
        struct node *head = 0, *n;
        struct pair p;
        used = 0;
        for (i = 0; i < 400; i++) { seed = seed * 69069u + 1; head = insert(head, alloc((int)(seed >> 20))); }
        for (n = head; n; n = n->next) sum += n->key + n->weight + n->tag;
        p = minmax(head);
        sum = sum * 3 + span(p) + p.lo;
    }
    report("lists", sum);
    return 0;
}
