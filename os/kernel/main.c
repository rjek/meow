/* Catflap starts here, on the boot stack, and ends up as the idle thread. */
#include "kernel.h"

extern char __bss_end[], __data_load[], __data_start[], __data_end[];

/* The ROM holds, after the kernel image: the library's relocation list
   if there is one, then the romfs, each word aligned. */
static const void *rom_after_image(void)
{
    uintptr_t end = (uintptr_t)__data_load + (uintptr_t)(__data_end - __data_start);

    return (const void *)((end + 3) & ~(uintptr_t)3);
}

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

    const uint32_t *rom = rom_after_image();
    const uint32_t *relocs = NULL;
    uint32_t nrelocs = 0;
    int i;

    console_init();
    alloc_init(__bss_end, heap_end);
    sched_init();
    kprintf("Catflap: %u KB RAM\n", ram / 1024);
    if (memcmp(rom, "CFRL", 4) == 0) {
        nrelocs = rom[1];
        relocs = rom + 2;
        rom += 2 + nrelocs;
    }
    process_init(relocs, nrelocs);
    if (vfs_mount("/", romfs_init(rom), "romfs") < 0 ||
        vfs_mount("/dev", devfs_init(), "devfs") < 0 ||
        vfs_mount("/proc", procfs_init(), "procfs") < 0 ||
        vfs_mount("/ipc", ipcfs_init(), "ipcfs") < 0) {
        kpanic("cannot mount");
    }
    strcpy(kproc.cwd, "/");
    {
        struct vnode *host = hostfs_init();

        if (host != NULL) {
            vfs_mount("/host", host, "hostfs");
        }
    }
    for (i = 0; i < 3; i++) {           /* the streams every process inherits */
        if (vfs_open("/dev/console", i == 0 ? O_RDONLY : O_WRONLY) != i) {
            kpanic("no console");
        }
    }
    sched_start();
    thread_create("init", init_thread, NULL, PRIO_INIT, STACK_DEFAULT);
    for (;;) {
        idle_work();
    }
}
