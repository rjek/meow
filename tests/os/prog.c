/* Stage 4: a program that uses the shared C library, while another
   process using it runs at the same time. */
#include "kernel.h"

void init_main(void)
{
    char *args[] = { "libctest", "arg1", "arg2", NULL };
    char *hargs[] = { "hello", NULL };
    int pid, hpid, status;

    hpid = process_spawn("/bin/hello", 1, hargs);
    pid = process_spawn("/bin/libctest", 3, args);
    process_wait(pid, &status);
    kprintf("libctest exited %d\n", status);
    process_wait(hpid, &status);
    kprintf("hello exited %d\n", status);
    kernel_halt(0);
}
