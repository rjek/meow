/* A test program with a thread on another CPU: says where each ran. */
#include <stdio.h>
#include "catflap.h"

static volatile int ran_on = -1;

static int other(void *arg)
{
    (void)arg;
    ran_on = cpu_id();
    return 0;
}

int main(int argc, char **argv)
{
    int cpu = argc > 1 ? argv[1][0] - '0' : 0;
    int rc;

    printf("main on cpu%d of %d\n", cpu_id(), cpu_count());
    rc = thread_spawn_on(other, NULL, 0, 4, cpu);
    if (rc < 0) {
        printf("thread_spawn_on: %d\n", rc);
        return 1;
    }
    while (ran_on < 0) {
        thread_yield();
    }
    printf("thread ran on cpu%d\n", ran_on);
    return 0;
}
