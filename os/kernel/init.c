/* What the kernel runs once it is up: /bin/init, and if that ends, a
   message and the idle loop. */
#include "kernel.h"

void init_main(void)
{
    char *argv[] = { "init", NULL };
    int pid = process_spawn("/bin/init", 1, argv), status;

    if (pid < 0) {
        kprintf("no /bin/init: %d\n", pid);
        return;
    }
    process_wait(pid, &status);
    kprintf("init exited with %d\n", status);
}
