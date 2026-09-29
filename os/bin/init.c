/* init: a shell on the console, again if it ends */
#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    char *args[] = { "sh", NULL };
    int pid, status;

    (void)argc;
    (void)argv;
    for (;;) {
        pid = process_spawn("/bin/sh", 1, args);
        if (pid < 0) {
            printf("init: cannot run /bin/sh: %d\n", pid);
            return 1;
        }
        process_wait(pid, &status);
        printf("init: sh exited with %d\n", status);
    }
}
