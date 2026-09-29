/* The first program: says hello through the kernel's own calls. */
#include "kernel.h"

static void say(const char *s)
{
    vfs_write(1, s, strlen(s));
}

int main(int argc, char **argv)
{
    int i;

    say("hello from a process, pid ");
    kprintf("%d", current_process()->pid);
    say(", args:");
    for (i = 0; i < argc; i++) {
        say(" ");
        say(argv[i]);
    }
    say("\n");
    return argc;
}
