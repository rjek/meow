/* Stage 4: programs loaded from romfs run as processes. */
#include "kernel.h"

void init_main(void)
{
    char *args[] = { "hello", "one", "two", NULL };
    char *args2[] = { "again", NULL };
    int pid, pid2, status;
    unsigned free0 = (unsigned)kmem_free();

    pid = process_spawn("/bin/hello", 3, args);
    kprintf("spawned pid %d\n", pid);
    kprintf("wait: %d status %d\n", process_wait(pid, &status), status);
    kprintf("no such program: %d\n", process_spawn("/bin/nothing", 0, args));
    kprintf("not a program: %d\n", process_spawn("/etc/motd", 0, args));
    pid = process_spawn("/bin/hello", 1, args2);
    pid2 = process_spawn("/bin/hello", 2, args);
    kprintf("two at once: %d %d\n", pid, pid2);
    process_wait(pid2, &status);
    kprintf("second done with %d\n", status);
    process_wait(pid, &status);
    kprintf("first done with %d, wait again: %d\n", status, process_wait(pid, &status));
    idle_work();                        /* reap what has finished */
    kprintf("%u bytes fewer free than at the start\n", free0 - (unsigned)kmem_free());
    kernel_halt(0);
}
