/* ls: a directory's names, with sizes; for a queue, the messages
   waiting, and for a semaphore, its count */
#include <stdio.h>
#include "catflap.h"

static int list(const char *path)
{
    struct cf_dirent de;
    int fd = vfs_open(path, CF_O_RDONLY);

    if (fd < 0) {
        fprintf(stderr, "ls: %s: error %d\n", path, -fd);
        return 1;
    }
    while (vfs_readdir(fd, &de) > 0) {
        switch (de.type) {
        case CF_V_DIR:
            printf("%8s %s/\n", "", de.name);
            break;
        case CF_V_DEV:
            printf("%8s %s\n", "dev", de.name);
            break;
        case CF_V_MQ:
            printf("%5u mq %s\n", de.size, de.name);
            break;
        case CF_V_SEM:
            printf("%4u sem %s\n", de.size, de.name);
            break;
        default:
            printf("%8u %s\n", de.size, de.name);
            break;
        }
    }
    vfs_close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    int i, rc = 0;

    if (argc == 1) {
        return list(".");
    }
    for (i = 1; i < argc; i++) {
        if (argc > 2) {
            printf("%s:\n", argv[i]);
        }
        rc |= list(argv[i]);
    }
    return rc;
}
