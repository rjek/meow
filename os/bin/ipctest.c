/* IPC from userland: threads sharing a queue of work, a second process
   talking over named queues, semaphores for start-up and completion.
   Only the main thread prints, since stdio keeps no locks. */
#include <stdio.h>
#include <string.h>
#include "catflap.h"

#define WORKERS 3
#define ITEMS 12

static int work, results;

/* sum the squares of the numbers on the work queue until a 0 arrives */
static int worker(void *arg)
{
    int n, sum = 0;

    (void)arg;
    for (;;) {
        vfs_read(work, &n, sizeof n);
        if (n == 0) {
            break;
        }
        sum += n * n;
        thread_yield();                 /* let the others have some */
    }
    vfs_write(results, &sum, sizeof sum);
    return 0;
}

static int open_or_die(const char *path)
{
    int fd = vfs_open(path, CF_O_RDWR);

    if (fd < 0) {
        printf("cannot open %s: %d\n", path, fd);
        process_exit(1);
    }
    return fd;
}

/* the other end: doubles what arrives on ping and answers on pong */
static int child(void)
{
    int ping = open_or_die("/ipc/ping"), pong = open_or_die("/ipc/pong");
    int ready = open_or_die("/ipc/ready"), n;
    char one = 1;

    vfs_write(ready, &one, 1);
    for (;;) {
        vfs_read(ping, &n, sizeof n);
        if (n < 0) {
            return 0;
        }
        n *= 2;
        vfs_write(pong, &n, sizeof n);
    }
}

static void list_ipc(void)
{
    struct cf_dirent de;
    int fd = vfs_open("/ipc", CF_O_RDONLY);

    printf("/ipc:");
    while (vfs_readdir(fd, &de) > 0) {
        printf(" %s(%s %u)", de.name, de.type == CF_V_MQ ? "mq" : "sem", de.size);
    }
    printf("\n");
    vfs_close(fd);
}

int main(int argc, char **argv)
{
    char *args[] = { "ipctest", "child", NULL };
    int i, n, total = 0, expect = 0, pid, status, ping, pong, ready, sem;
    char buf[1];

    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        return child();
    }

    /* threads and a work queue */
    printf("create: %d", ipc_create("work", CF_IPC_MQ, sizeof(int), 4));
    printf(" %d", ipc_create("results", CF_IPC_MQ, sizeof(int), WORKERS));
    printf(", again: %d", ipc_create("work", CF_IPC_MQ, sizeof(int), 4));
    printf(", bad size: %d\n", ipc_create("bad", CF_IPC_MQ, 0, 4));
    work = open_or_die("/ipc/work");
    results = open_or_die("/ipc/results");
    for (i = 0; i < WORKERS; i++) {
        if (thread_spawn(worker, NULL, 0, 0) <= 0) {
            printf("cannot start a thread\n");
            return 1;
        }
    }
    for (i = 1; i <= ITEMS; i++) {
        vfs_write(work, &i, sizeof i);  /* blocks while four are waiting */
        expect += i * i;
    }
    n = 0;
    for (i = 0; i < WORKERS; i++) {
        vfs_write(work, &n, sizeof n);
    }
    for (i = 0; i < WORKERS; i++) {
        vfs_read(results, &n, sizeof n);
        total += n;
    }
    printf("threads: sum of squares %d, expected %d\n", total, expect);
    printf("short write: %d", vfs_write(work, buf, 1));
    printf(", try on empty: %d\n", vfs_ioctl(work, CF_IPC_TRYRECV, &n));

    /* a second process over named queues, started by a semaphore */
    ipc_create("ping", CF_IPC_MQ, sizeof(int), 2);
    ipc_create("pong", CF_IPC_MQ, sizeof(int), 2);
    ipc_create("ready", CF_IPC_SEM, 0, 0);
    list_ipc();
    ping = open_or_die("/ipc/ping");
    pong = open_or_die("/ipc/pong");
    ready = open_or_die("/ipc/ready");
    printf("trywait before: %d\n", vfs_ioctl(ready, CF_IPC_TRYWAIT, NULL));
    pid = process_spawn("/bin/ipctest", 2, args);
    vfs_read(ready, buf, 1);
    printf("child %s ready\n", pid > 0 ? "is" : "is not");
    for (i = 1; i <= 5; i++) {
        vfs_write(ping, &i, sizeof i);
        vfs_read(pong, &n, sizeof n);
        printf("%d->%d ", i, n);
    }
    printf("\n");
    n = -1;
    vfs_write(ping, &n, sizeof n);
    process_wait(pid, &status);
    printf("child exited %d\n", status);

    /* a semaphore as a count */
    ipc_create("count", CF_IPC_SEM, 2, 0);
    sem = open_or_die("/ipc/count");
    printf("count %d, take", vfs_ioctl(sem, CF_IPC_VALUE, NULL));
    for (i = 0; i < 3; i++) {
        printf(" %d", vfs_ioctl(sem, CF_IPC_TRYWAIT, NULL));
    }
    printf(", ");
    vfs_write(sem, buf, 1);
    printf("after a post %d\n", vfs_ioctl(sem, CF_IPC_VALUE, NULL));

    /* names go; the objects last while open */
    vfs_unlink("/ipc/work");
    vfs_unlink("/ipc/results");
    vfs_unlink("/ipc/ping");
    vfs_unlink("/ipc/pong");
    vfs_unlink("/ipc/ready");
    vfs_unlink("/ipc/count");
    list_ipc();
    n = 42;
    vfs_write(results, &n, sizeof n);
    n = 0;
    vfs_read(results, &n, sizeof n);
    printf("unlinked but open: %d\n", n);
    return 0;
}
