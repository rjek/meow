/* ps: the processes, from /proc */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catflap.h"

/* the value of a "key value" line in a /proc file */
static int field(const char *path, const char *key, char *out, size_t size)
{
    char line[80];
    size_t klen = strlen(key);
    FILE *f = fopen(path, "r");
    int found = 0;

    if (f == NULL) {
        return 0;
    }
    while (found == 0 && fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, key, klen) == 0 && line[klen] == ' ') {
            line[strcspn(line, "\n")] = '\0';
            strncpy(out, line + klen + 1, size - 1);
            out[size - 1] = '\0';
            found = 1;
        }
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv)
{
    struct cf_dirent de;
    int fd = vfs_open("/proc", CF_O_RDONLY);

    (void)argc;
    (void)argv;
    if (fd < 0) {
        fprintf(stderr, "ps: no /proc\n");
        return 1;
    }
    printf("  pid parent threads name\n");
    while (vfs_readdir(fd, &de) > 0) {
        char path[64], name[32], parent[16], threads[16], state[16];

        if (de.name[0] < '0' || de.name[0] > '9') {
            continue;
        }
        sprintf(path, "/proc/%s/status", de.name);
        if (field(path, "name", name, sizeof name) == 0) {
            continue;                   /* ended since the listing */
        }
        field(path, "parent", parent, sizeof parent);
        field(path, "threads", threads, sizeof threads);
        field(path, "state", state, sizeof state);
        printf("%5s %6s %7s %s%s\n", de.name, parent, threads, name,
               strcmp(state, "done") == 0 ? " (done)" : "");
    }
    vfs_close(fd);
    return 0;
}
