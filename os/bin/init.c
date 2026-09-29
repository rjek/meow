/* init: runs the programs the boot wants run.  A shell later. */
#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    char *args[] = { "hello", "from", "init", NULL };
    int pid, status;

    (void)argc;
    (void)argv;
    pid = process_spawn("/bin/hello", 3, args);
    if (pid < 0) {
        printf("init: cannot run /bin/hello: %d\n", pid);
        return 1;
    }
    process_wait(pid, &status);
    return status;
}
