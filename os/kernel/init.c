/* What the kernel runs once it is up.  Stage 1: not much. */
#include "kernel.h"

static int hello(void *arg)
{
    kprintf("hello from thread %s\n", (const char *)arg);
    return 0;
}

void init_main(void)
{
    thread_create("hello", hello, "one", PRIO_DEFAULT, STACK_DEFAULT);
}
