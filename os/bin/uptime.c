#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    unsigned t = ticks_now();

    (void)argc;
    (void)argv;
    printf("up %u.%02u seconds\n", t / 100, t % 100);
    return 0;
}
