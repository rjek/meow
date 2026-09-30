/* Stage 5: a scripted shell session. */
#include "kernel.h"
#include "memfs.h"

void init_main(void)
{
    char *args[] = { "sh", NULL };
    int pid, status;

    start_memfs();
    pid = process_spawn("/bin/sh", 1, args);
    process_wait(pid, &status);
    kprintf("sh exited %d\n", status);
    kernel_halt(0);
}
