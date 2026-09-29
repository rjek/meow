/* Catflap starts here, on the boot stack, and ends up as the idle thread. */
#include "kernel.h"

extern char __bss_end[];

/* init_main runs above everything it starts, so that it finishes
   starting things before any of them runs */
static int init_thread(void *arg)
{
    (void)arg;
    init_main();
    return 0;
}

void kmain(void)
{
    uint32_t ram = CH_CS_SIZE(1);
    char *heap_end = (char *)RAM_BASE + ram - BOOT_STACK - IRQ_STACK;

    console_init();
    alloc_init(__bss_end, heap_end);
    sched_init();
    kprintf("Catflap: %u KB RAM, %u KB free\n", ram / 1024,
            (unsigned)kmem_free() / 1024);
    sched_start();
    thread_create("init", init_thread, NULL, PRIO_INIT, STACK_DEFAULT);
    for (;;) {
        idle_work();
    }
}
