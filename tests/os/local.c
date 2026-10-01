/* Stage 12, continued: an exclusive thread with its stack and code in
   its CPU's local memory, and a program that bit-bangs a UART from one. */
#include "kernel.h"

static struct sem done;
static volatile unsigned where;

static int local_worker(void *arg)
{
    unsigned here = (unsigned)(uintptr_t)&here;

    (void)arg;
    where = here;
    return 0;
}

static int local_worker_end(void *arg)
{
    (void)arg;
    return 0;
}

static int waiter(void *arg)
{
    (void)arg;
    while (where == 0) {
        thread_yield();
    }
    sem_post(&done);
    return 0;
}

void init_main(void)
{
    size_t size = (size_t)((char *)local_worker_end - (char *)local_worker);
    struct thread *t;
    char *const bitbang[] = { "bitbang", "hello from cpu1, locally\n" };
    int pid, status;

    sem_init(&done, 0);
    kprintf("local memory %u bytes a CPU\n", (unsigned)CH_CS_SIZE(30));
    t = thread_create_local(&kproc, "local", local_worker, size, NULL, 5, 1);
    if (t == NULL) {
        kprintf("no local thread\n");
        kernel_halt(1);
    }
    kprintf("code %s, stack %s, %u bytes of it\n",
            t->regs[R_PC] == LOCAL_BASE + LOCAL_CODE ? "copied to local memory" : "elsewhere",
            (uint32_t)t->stack >= LOCAL_BASE ? "in local memory" : "elsewhere",
            (unsigned)t->stack_size);
    kprintf("copy %s the original\n",
            memcmp((char *)cpu_of(1) + LOCAL_CODE, (const void *)local_worker, size) == 0 ?
            "matches" : "differs from");
    thread_create("waiter", waiter, NULL, 5, STACK_DEFAULT);
    sem_wait(&done);
    kprintf("its local variable was %s\n", where >= LOCAL_BASE && where < LOCAL_BASE + CH_CS_SIZE(30) ?
            "in local memory" : "elsewhere");
    thread_sleep(1);
    kprintf("cpu1 is %s again\n", cpu_of(1)->exclusive == NULL ? "free" : "taken");

    pid = process_spawn("/bin/bitbang", 2, bitbang);
    process_wait(pid, &status);
    kprintf("bitbang exited %d\n", status);
    kernel_halt(0);
}
