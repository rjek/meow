/* ls: a directory's names, with sizes */
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
        if (de.type == CF_V_DIR) {
            printf("%8s %s/\n", "", de.name);
        } else if (de.type == CF_V_DEV) {
            printf("%8s %s\n", "dev", de.name);
        } else {
            printf("%8u %s\n", de.size, de.name);
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
