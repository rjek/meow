/* The first program: says hello through the shared C library. */
#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    int i;

    printf("hello from a process, pid %d, args:", process_pid());
    for (i = 0; i < argc; i++) {
        printf(" %s", argv[i]);
    }
    printf("\n");
    return argc;
}
