/* Shared by the benchmarks: console output through msim and a one-line
 * result the runner checks against the host compiler's answer. */
#include "msim.h"

static void report(const char *name, unsigned sum)
{
    puts(name);
    print_hex(sum);
    putchar('\n');
}
