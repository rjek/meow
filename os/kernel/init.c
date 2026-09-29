/* What the kernel runs once it is up: /bin/init, and when that ends the
   machine stops. */
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
    if (status != 0) {
        kprintf("init exited with %d\n", status);
    }
    kernel_halt(status);
}
