/* mount: the mounted file systems.  Nothing is mounted from here: the
   kernel mounts what it finds at boot, and a file system that is a
   program, memfs for one, mounts itself when it is run. */
#include <stdio.h>

int main(int argc, char **argv)
{
    char path[64], type[16];
    FILE *f = fopen("/proc/mounts", "r");

    if (argc > 1) {
        fprintf(stderr, "mount: this only lists; a file system server mounts itself\n");
        return 2;
    }
    (void)argv;
    if (f == NULL) {
        fprintf(stderr, "mount: cannot read /proc/mounts\n");
        return 1;
    }
    while (fscanf(f, "%63s %15s", path, type) == 2) {
        printf("%s on %s\n", type, path);
    }
    fclose(f);
    return 0;
}
