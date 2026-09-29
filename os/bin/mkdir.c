#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    int i, rc = 0;

    if (argc < 2) {
        fprintf(stderr, "usage: mkdir directory...\n");
        return 2;
    }
    for (i = 1; i < argc; i++) {
        int e = vfs_mkdir(argv[i]);

        if (e < 0) {
            fprintf(stderr, "mkdir: %s: error %d\n", argv[i], -e);
            rc = 1;
        }
    }
    return rc;
}
