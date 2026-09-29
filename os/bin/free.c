/* free: memory, from /proc/meminfo */
#include <stdio.h>

int main(int argc, char **argv)
{
    unsigned total = 0, free_ = 0;
    FILE *f = fopen("/proc/meminfo", "r");

    (void)argc;
    (void)argv;
    if (f == NULL || fscanf(f, "total %u free %u", &total, &free_) != 2) {
        fprintf(stderr, "free: cannot read /proc/meminfo\n");
        return 1;
    }
    fclose(f);
    printf("%u bytes of RAM, %u free\n", total, free_);
    return 0;
}
