/* uname: from /proc/version, "system release machine implementation";
   -s, -r, -m and -a choose, -s being the default */
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    char sys[16], rel[16], mach[16], impl[16];
    FILE *f = fopen("/proc/version", "r");
    int i;

    if (f == NULL || fscanf(f, "%15s %15s %15s %15s", sys, rel, mach, impl) != 4) {
        fprintf(stderr, "uname: cannot read /proc/version\n");
        return 1;
    }
    fclose(f);
    if (argc == 1) {
        printf("%s\n", sys);
        return 0;
    }
    for (i = 1; i < argc; i++) {
        const char *s = strcmp(argv[i], "-s") == 0 ? sys : strcmp(argv[i], "-r") == 0 ? rel :
                        strcmp(argv[i], "-m") == 0 ? mach : NULL;

        if (strcmp(argv[i], "-a") == 0) {
            printf("%s%s %s %s %s", i > 1 ? " " : "", sys, rel, mach, impl);
        } else if (s != NULL) {
            printf("%s%s", i > 1 ? " " : "", s);
        } else {
            fprintf(stderr, "usage: uname [-s] [-r] [-m] [-a]\n");
            return 2;
        }
    }
    printf("\n");
    return 0;
}
