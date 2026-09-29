#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    struct cf_procinfo p;
    int i;

    (void)argc;
    (void)argv;
    printf("  pid parent threads name\n");
    for (i = 0; process_info(i, &p) > 0; i++) {
        printf("%5d %6d %7d %s%s\n", p.pid, p.parent, p.nthreads, p.name, p.dead ? " (done)" : "");
    }
    return 0;
}
