/* Serving files from a program, polling descriptors, and killing
   processes.  Run with no arguments this is the test; with "box" it is
   a device server, and with "reader" or "threads" a client that waits
   on it, for the test to kill or to see end.

   /dev/box holds one message.  A write replaces it; a read takes it, or
   if there is none waits for the next write, which the server does by
   keeping the read's request until then.  Its ioctls are for the test. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catflap.h"

enum { BOX_SERVED = 1, BOX_CANCELS, BOX_HANG, BOX_SELF, BOX_EXIT, BOX_WAITING,
       BOX_RELEASES, BOX_IDLE, BOX_BAD };
#define BOX 1
#define LIMIT 30                        /* ticks the server may be away */

/* ---- the server ---- */

static char held[64];
static unsigned count;
static struct cf_req *reader;           /* a read waiting for a message */

static void ready(int port)
{
    srv_ready(port, BOX, CF_POLLOUT | (count != 0 ? CF_POLLIN : 0));
}

static int box(void)
{
    struct cf_req *r;
    struct cf_pollfd p;
    int port = srv_create(LIMIT, CF_SRV_RELEASE), served = 0, cancels = 0, releases = 0;
    int idle = srv_recv(port, &r, 2);   /* nobody knows of it yet: 0 when the time is up */

    if (port < 0 || srv_dev(port, "box", BOX) < 0) {
        return 1;
    }
    ready(port);
    p.fd = port;
    p.events = CF_POLLIN;
    for (;;) {
        int result = -ENOTTY;

        /* wait as a server with other descriptors to watch would */
        if (vfs_poll(&p, 1, -1) < 1 || srv_recv(port, &r, 0) < 1) {
            continue;
        }
        if (r->cancelled != 0) {        /* the reader we kept is being killed */
            reader = NULL;
            cancels++;
            srv_reply(port, r, -EINTR);
            continue;
        }
        served++;
        switch (r->op) {
        case CF_OP_RELEASE:
            releases++;
            result = 0;
            break;
        case CF_OP_WRITE:
            result = r->len < sizeof held ? (int)r->len : (int)sizeof held;
            if (reader != NULL) {       /* straight to whoever is waiting */
                memcpy(reader->buf, r->buf, (size_t)result);
                srv_reply(port, reader, result);
                reader = NULL;
            } else {
                memcpy(held, r->buf, (size_t)result);
                count = (unsigned)result;
            }
            break;
        case CF_OP_READ:
            if (count == 0) {
                if (reader != NULL) {
                    result = -EAGAIN;
                    break;
                }
                reader = r;             /* answered by the next write */
                continue;
            }
            result = r->len < count ? (int)r->len : (int)count;
            memcpy(r->buf, held, (size_t)result);
            count = 0;
            break;
        case CF_OP_IOCTL:
            switch (r->off) {
            case BOX_SERVED: result = served; break;
            case BOX_CANCELS: result = cancels; break;
            case BOX_WAITING: result = reader != NULL; break;
            case BOX_RELEASES: result = releases; break;
            case BOX_IDLE: result = idle; break;
            case BOX_BAD: result = srv_reply(port, (struct cf_req *)held, 0); break;
            case BOX_SELF:              /* which would be to wait for itself */
                result = vfs_open("/dev/box", CF_O_RDWR);
                result = vfs_read(result, held, 1);
                break;
            case BOX_EXIT: return 7;    /* with this request, and perhaps a reader, unanswered */
            case BOX_HANG:
                for (;;) {
                    thread_sleep(1000);
                }
            }
            break;
        }
        srv_reply(port, r, result);
        ready(port);
    }
}

/* ---- a client that reads the box and says what it got ---- */

static int read_box(void)
{
    char buf[16];
    int fd = vfs_open("/dev/box", CF_O_RDWR);
    int n = vfs_read(fd, buf, sizeof buf);

    printf("reader: %d\n", n);
    return 0;
}

/* one thread waits on the box while the first ends the program */
static int reads(void *arg)
{
    char buf[16];

    vfs_read(*(int *)arg, buf, sizeof buf);
    return 0;
}

static int threads(void)
{
    int fd = vfs_open("/dev/box", CF_O_RDWR), i;

    thread_spawn(reads, &fd, 0, 0);
    for (i = 0; i < 1000 && vfs_ioctl(fd, BOX_WAITING, NULL) == 0; i++) {
        thread_sleep(1);
    }
    return 5;
}

/* ---- the test ---- */

static int fd;
static const char *late;
static unsigned delay = 5;

static int writer(void *arg)
{
    (void)arg;
    thread_sleep(delay);
    vfs_write(fd, late, (unsigned)strlen(late));
    return 0;
}

static int start(const char *mode, const char *path)
{
    char *args[] = { "srvtest", (char *)mode, NULL };
    struct cf_stat st;
    int pid = process_spawn("/bin/srvtest", 2, args), i;

    for (i = 0; path != NULL && i < 1000 && vfs_stat(path, &st) < 0; i++) {
        thread_sleep(1);
    }
    return pid;
}

static void wait_for_reader(void)
{
    int i;

    for (i = 0; i < 1000 && vfs_ioctl(fd, BOX_WAITING, NULL) == 0; i++) {
        thread_sleep(1);
    }
}

static int poll1(int d, int events, int ticks)
{
    struct cf_pollfd p;
    int n;

    p.fd = d;
    p.events = (short)events;
    n = vfs_poll(&p, 1, ticks);
    return n * 100 + p.revents;         /* how many, and what */
}

static int status_of(int pid)
{
    int status = -1;

    process_wait(pid, &status);
    return status;
}

static void files(void)
{
    char *memfs[] = { "memfs", "/mnt", NULL }, *hello[] = { "/mnt/hello", "from", "memfs", NULL };
    char buf[300];
    struct cf_stat st;
    struct cf_dirent de;
    int server = process_spawn("/bin/memfs", 2, memfs), f, g, n, i, a, b;

    for (i = 0; i < 1000 && vfs_stat("/mnt", &st) < 0; i++) {
        thread_sleep(1);
    }
    a = vfs_mkdir("/mnt/d");
    b = vfs_mkdir("/mnt/d");
    printf("mkdir %d again %d", a, b);
    f = vfs_open("/mnt/d/f", CF_O_RDWR | CF_O_CREAT);
    n = vfs_write(f, "one two three\n", 14);
    printf(", wrote %d", n);
    vfs_seek(f, 4, CF_SEEK_SET);
    memset(buf, 0, sizeof buf);
    n = vfs_read(f, buf, 3);
    printf(", read %d '%s'", n, buf);
    printf(", end at %d\n", vfs_seek(f, 0, CF_SEEK_END));
    vfs_close(f);
    f = vfs_open("/mnt/d/f", CF_O_WRONLY | CF_O_APPEND);
    vfs_write(f, "four\n", 5);
    vfs_close(f);
    vfs_stat("/mnt/d/f", &st);
    printf("appended: size %u, missing %d", st.size, vfs_stat("/mnt/nothing", &st));
    printf(", remove full %d", vfs_unlink("/mnt/d"));
    f = vfs_open("/mnt/d", CF_O_RDONLY);
    while (vfs_readdir(f, &de) > 0) {
        printf(", %s(%u)", de.name, de.size);
    }
    vfs_close(f);
    a = vfs_unlink("/mnt/d/f");
    b = vfs_unlink("/mnt/d");
    printf(", remove %d %d\n", a, b);

    /* a program copied there runs from there */
    f = vfs_open("/bin/hello", CF_O_RDONLY);
    g = vfs_open("/mnt/hello", CF_O_WRONLY | CF_O_CREAT);
    while ((n = vfs_read(f, buf, sizeof buf)) > 0) {
        vfs_write(g, buf, (unsigned)n);
    }
    vfs_close(f);
    vfs_close(g);
    fflush(stdout);
    printf("it exited %d\n", status_of(process_spawn("/mnt/hello", 3, hello)));

    f = vfs_open("/mnt/hello", CF_O_RDONLY);
    a = process_kill(server);
    printf("kill %d: %d", a, status_of(server));
    a = vfs_read(f, buf, 1);
    printf(", then read %d, stat %d\n", a, vfs_stat("/mnt", &st));
    vfs_close(f);
}

static int test(void)
{
    char *sleep[] = { "sleep", "100000", NULL };
    char buf[32];
    int server, child, p[2], n, a, b;

    server = start("box", "/dev/box");
    fd = vfs_open("/dev/box", CF_O_RDWR);
    n = vfs_write(fd, "hello", 5);
    memset(buf, 0, sizeof buf);
    printf("write %d", n);
    n = vfs_read(fd, buf, sizeof buf);
    printf(", read %d '%s'\n", n, buf);

    a = poll1(fd, CF_POLLIN, 0);
    printf("poll empty: in %d, out %d", a, poll1(fd, CF_POLLIN | CF_POLLOUT, 0));
    vfs_write(fd, "x", 1);
    printf("; full: in %d", poll1(fd, CF_POLLIN, 0));
    vfs_read(fd, buf, sizeof buf);
    printf("; three ticks: %d", poll1(fd, CF_POLLIN, 3));
    late = "late";
    thread_spawn(writer, NULL, 0, 0);
    printf("; until written: %d\n", poll1(fd, CF_POLLIN, 500));
    vfs_read(fd, buf, sizeof buf);

    late = "later";
    delay = 2 * LIMIT;                  /* a request that is kept has no time limit */
    thread_spawn(writer, NULL, 0, 0);
    memset(buf, 0, sizeof buf);
    n = vfs_read(fd, buf, sizeof buf);  /* the server keeps this until the write */
    printf("a read that waited: %d '%s'\n", n, buf);
    printf("the server reading its own device: %d", vfs_ioctl(fd, BOX_SELF, NULL));
    a = vfs_ioctl(fd, BOX_RELEASES, NULL);
    vfs_close(vfs_open("/dev/box", CF_O_RDWR));
    printf(", releases for an open and close: %d", vfs_ioctl(fd, BOX_RELEASES, NULL) - a);
    a = vfs_ioctl(fd, BOX_IDLE, NULL);
    printf(", a receive with nothing to take: %d, a bad reply: %d\n", a, vfs_ioctl(fd, BOX_BAD, NULL));

    vfs_pipe(p);
    printf("pipe: empty %d", poll1(p[0], CF_POLLIN, 0));
    vfs_write(p[1], "abc", 3);
    a = poll1(p[0], CF_POLLIN, 0);
    printf(", written %d, room %d", a, poll1(p[1], CF_POLLOUT, 0));
    vfs_close(p[1]);
    vfs_read(p[0], buf, sizeof buf);
    a = poll1(p[0], CF_POLLIN, 0);
    printf(", writer gone %d, closed %d\n", a, poll1(p[1], CF_POLLIN, 0));
    vfs_close(p[0]);

    a = process_kill(9999);
    b = process_kill(1);
    printf("kill: nobody %d, init %d, the kernel %d\n", a, b, process_kill(0));
    child = process_spawn("/bin/sleep", 2, sleep);
    thread_sleep(2);
    a = process_kill(child);
    printf("kill a sleeper %d: %d\n", a, status_of(child));

    /* a client killed while the server holds its request */
    fflush(stdout);
    child = start("reader", NULL);
    wait_for_reader();
    printf("kill a reader %d", process_kill(child));
    printf(": %d", status_of(child));
    a = vfs_ioctl(fd, BOX_CANCELS, NULL);
    printf(", cancellations %d, still waiting %d\n", a, vfs_ioctl(fd, BOX_WAITING, NULL));

    /* a thread waiting on the server when its program ends */
    child = start("threads", NULL);
    printf("a program ending under a waiting thread: %d", status_of(child));
    a = vfs_ioctl(fd, BOX_CANCELS, NULL);
    printf(", cancellations %d, still waiting %d\n", a, vfs_ioctl(fd, BOX_WAITING, NULL));

    /* the server ends with requests unanswered */
    fflush(stdout);
    child = start("reader", NULL);
    wait_for_reader();
    printf("exit: %d\n", vfs_ioctl(fd, BOX_EXIT, NULL));
    fflush(stdout);
    status_of(child);
    printf("server %d", status_of(server));
    a = vfs_write(fd, "x", 1);
    b = poll1(fd, CF_POLLIN, 0);
    printf(", after it write %d, poll %d, open %d\n", a, b, vfs_open("/dev/box", CF_O_RDWR));
    vfs_close(fd);

    /* the server stops answering */
    server = start("box", "/dev/box");
    fd = vfs_open("/dev/box", CF_O_RDWR);
    printf("hang: %d", vfs_ioctl(fd, BOX_HANG, NULL));
    printf(", server %d", status_of(server));
    printf(", open %d\n", vfs_open("/dev/box", CF_O_RDWR));
    vfs_close(fd);

    files();
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "box") == 0) {
        return box();
    }
    if (argc > 1 && strcmp(argv[1], "reader") == 0) {
        return read_box();
    }
    if (argc > 1 && strcmp(argv[1], "threads") == 0) {
        return threads();
    }
    return test();
}
