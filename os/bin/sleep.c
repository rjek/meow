#include <stdlib.h>
#include "catflap.h"

int main(int argc, char **argv)
{
    thread_sleep(argc > 1 ? (unsigned)atoi(argv[1]) : 100);
    return 0;
}
