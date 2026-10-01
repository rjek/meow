/* Stage 12: threads on another CPU: they share the kernel's semaphores
   and mutexes, sleep, run programs through the shared C library, are
   killed, and can have the CPU to themselves. */
#include "kernel.h"

static struct sem ping, pong, done;
static struct mutex m;
static int shared;
static volatile int release;

static int worker(void *arg)
{
    int i;

    (void)arg;
    kprintf("worker on cpu%d\n", cpu_id());
    for (i = 0; i < 5; i++) {
        sem_wait(&ping);
        mutex_lock(&m);
        shared++;
        mutex_unlock(&m);
        kprintf("worker: shared %d\n", shared);
        sem_post(&pong);
    }
    return 0;
}

static int napper(void *arg)
{
    uint32_t t0 = ticks_now();

    (void)arg;
    thread_sleep(3);
    kprintf("napper on cpu%d slept %u ticks\n", cpu_id(), ticks_now() - t0);
    sem_post(&done);
    return 0;
}

static int exclusive(void *arg)
{
    (void)arg;
    while (release == 0) {              /* nothing interrupts this but the doorbell */
    }
    kprintf("exclusive on cpu%d let go\n", cpu_id());
    sem_post(&done);
    return 0;
}

static void show(const char *path)
{
    char buf[128];
    int fd = vfs_open(path, O_RDONLY), n;

    if (fd >= 0) {
        n = vfs_read(fd, buf, sizeof buf - 1);
        buf[n > 0 ? n : 0] = '\0';
        kprintf("%s", buf);
        vfs_close(fd);
    }
}

void init_main(void)
{
    char *const hello[] = { "hello", "there" };
    char *const spin[] = { "spin" };
    char *const cputest[] = { "cputest", "1" };
    int i, pid, status;

    kprintf("init on cpu%d of %d\n", cpu_id(), cpu_count());
    sem_init(&ping, 0);
    sem_init(&pong, 0);
    sem_init(&done, 0);
    mutex_init(&m);

    thread_create_on("worker", worker, NULL, 5, STACK_DEFAULT, 1);
    for (i = 0; i < 5; i++) {
        mutex_lock(&m);
        shared += 10;
        mutex_unlock(&m);
        sem_post(&ping);
        sem_wait(&pong);
        kprintf("init: shared %d\n", shared);
    }

    thread_create_on("napper", napper, NULL, 5, STACK_DEFAULT, 1);
    sem_wait(&done);

    pid = process_spawn_on("/bin/hello", 2, hello, 1);
    process_wait(pid, &status);
    kprintf("hello on cpu1 exited %d\n", status);

    pid = process_spawn_on("/bin/cputest", 2, cputest, 0);
    process_wait(pid, &status);
    kprintf("cputest exited %d\n", status);

    pid = process_spawn_on("/bin/spin", 1, spin, 1);
    thread_sleep(2);
    show("/proc/cpus");
    kprintf("kill: %d\n", process_kill(pid));
    process_wait(pid, &status);
    kprintf("spin exited %d\n", status);

    if (thread_create_on("excl", exclusive, NULL, 5, STACK_DEFAULT, 1 | CPU_EXCLUSIVE) == NULL) {
        kprintf("no exclusive thread\n");
    }
    thread_sleep(2);
    show("/proc/cpus");
    kprintf("another on cpu1 while exclusive: %s\n",
            thread_create_on("late", napper, NULL, 5, STACK_DEFAULT, 1) == NULL ? "refused" : "made");
    release = 1;
    sem_wait(&done);
    thread_sleep(1);
    show("/proc/cpus");
    kprintf("%s free\n", kmem_free() > 200 * 1024 ? "plenty" : "little");
    kernel_halt(0);
}
