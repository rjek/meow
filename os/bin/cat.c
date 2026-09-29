/* cat: files, or standard input, to standard output */
#include <stdio.h>

static int copy(FILE *f)
{
    char buf[256];
    size_t n;

    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i, rc = 0;

    if (argc == 1) {
        return copy(stdin);
    }
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");

        if (f == NULL) {
            fprintf(stderr, "cat: %s: cannot open\n", argv[i]);
            rc = 1;
            continue;
        }
        copy(f);
        fclose(f);
    }
    return rc;
}
