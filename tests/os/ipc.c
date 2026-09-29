/* Userland IPC: /bin/ipctest runs threads and a second process. */
#include "kernel.h"

void init_main(void)
{
    char *args[] = { "ipctest", NULL };
    int pid, status;
    unsigned before = (unsigned)kmem_free();

    pid = process_spawn("/bin/ipctest", 1, args);
    process_wait(pid, &status);
    kprintf("ipctest exited %d\n", status);
    idle_work();
    kprintf("%u bytes fewer free than before\n", before - (unsigned)kmem_free());
    kernel_halt(0);
}
