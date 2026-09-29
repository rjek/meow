/* init: a shell on the console, again if it fails.  A shell that ends
   cleanly, by exit or at the end of input, shuts the system down. */
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
        if (status == 0) {
            return 0;
        }
        printf("init: sh exited with %d, starting another\n", status);
    }
}
