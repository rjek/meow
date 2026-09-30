/* Ports, poll and kill: /bin/srvtest serves a device, polls it, kills
   and is killed, and uses /bin/memfs as a file system. */
#include "kernel.h"

void init_main(void)
{
    char *args[] = { "srvtest", NULL };
    int pid, status;
    unsigned before = (unsigned)kmem_free();

    pid = process_spawn("/bin/srvtest", 1, args);
    process_wait(pid, &status);
    kprintf("srvtest exited %d\n", status);
    idle_work();
    kprintf("%u bytes fewer free than before\n", before - (unsigned)kmem_free());
    kernel_halt(0);
}
