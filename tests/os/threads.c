/* Stage 1: threads sleep, are preempted, and outrank each other. */
#include "kernel.h"

static uint32_t t0;                     /* the tick the test started on */

static int sleeper(void *arg)
{
    int n = (int)arg, i;

    for (i = 0; i < 3; i++) {
        kprintf("sleeper %d step %d at tick %u\n", n, i, ticks_now() - t0);
        thread_sleep((uint32_t)n);
    }
    return n;
}

/* two of these at one priority: only preemption lets the second finish */
static int spinner(void *arg)
{
    uint32_t until = (uint32_t)arg;

    while (ticks_now() - t0 < until) {
    }
    kprintf("spinner until %u done\n", until);
    return 0;
}

static int boss(void *arg)
{
    (void)arg;
    thread_sleep(12);
    kprintf("boss: %s free, tick %u\n", kmem_free() > 200 * 1024 ? "plenty" : "little", ticks_now() - t0);
    kernel_halt(0);
    return 0;
}

void init_main(void)
{
    t0 = ticks_now();
    while (ticks_now() == t0) {         /* start on a tick, whatever booting cost */
    }
    t0++;
    thread_create("s1", sleeper, (void *)1, 5, STACK_DEFAULT);
    thread_create("s2", sleeper, (void *)2, 5, STACK_DEFAULT);
    thread_create("s3", sleeper, (void *)3, 5, STACK_DEFAULT);
    thread_create("spin8", spinner, (void *)8, 4, STACK_DEFAULT);
    thread_create("spin5", spinner, (void *)5, 4, STACK_DEFAULT);
    thread_create("boss", boss, NULL, 6, STACK_DEFAULT);
    kprintf("init: all started at tick %u\n", ticks_now() - t0);
}
