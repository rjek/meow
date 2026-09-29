/* Programs run in place from ROM, and copied from elsewhere: from
   /host, and from a copy in /tmp of the prelinked file in ROM. */
#include "kernel.h"

static int run(char *path, char *arg)
{
    char *argv[] = { path, arg, NULL };
    int pid = process_spawn(path, 2, argv), status = -1;
    struct process *p = process_find(pid);

    if (pid < 0) {
        kprintf("%s: %d\n", path, pid);
        return pid;
    }
    kprintf("%s: code %s\n", path, p->image == NULL ? "in ROM" : "in RAM");
    process_wait(pid, &status);
    kprintf("%s exited %d\n", path, status);
    return status;
}

static void copy(const char *from, const char *to)
{
    char buf[256];
    int in = vfs_open(from, O_RDONLY), out = vfs_open(to, O_WRONLY | O_CREAT | O_TRUNC), n;

    while ((n = vfs_read(in, buf, sizeof buf)) > 0) {
        vfs_write(out, buf, (size_t)n);
    }
    vfs_close(in);
    vfs_close(out);
}

void init_main(void)
{
    unsigned before;

    run("/bin/libctest", "rom");
    run("/host/bin/libctest", "host");
    copy("/bin/libctest", "/tmp/libctest");
    run("/tmp/libctest", "tmp");
    copy("/etc/motd", "/tmp/notaprogram");
    run("/tmp/notaprogram", "x");
    idle_work();
    before = (unsigned)kmem_free();
    run("/bin/hello", "again");
    idle_work();
    kprintf("%u bytes fewer free than before\n", before - (unsigned)kmem_free());
    kernel_halt(0);
}
