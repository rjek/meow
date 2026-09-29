#include <stdio.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("%u bytes free\n", kmem_free());
    return 0;
}
