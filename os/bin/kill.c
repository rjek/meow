/* kill: end processes, by pid */
#include <stdio.h>
#include <stdlib.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    int i, rc = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: kill pid...\n");
        return 2;
    }
    for (i = 1; i < argc; i++) {
        int e = process_kill(atoi(argv[i]));

        if (e < 0) {
            fprintf(stderr, "kill: %s: error %d\n", argv[i], -e);
            rc = 1;
        }
    }
    return rc;
}
