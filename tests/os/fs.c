/* Stage 5: pipes, ramfs, dup2, the working directory. */
#include "kernel.h"

static int fds[2];

static int writer(void *arg)
{
    int i, n = 0;
    char buf[40];

    (void)arg;
    for (i = 0; i < 50; i++) {          /* well over the pipe's size */
        int k, len = 0;

        for (k = 0; k < 20; k++) {
            buf[len++] = (char)('a' + (i + k) % 26);
        }
        buf[len++] = '\n';
        n += vfs_write(fds[1], buf, (size_t)len);
    }
    vfs_close(fds[1]);
    kprintf("writer: %d bytes\n", n);
    return 0;
}

static void list(const char *path)
{
    int fd = vfs_open(path, O_RDONLY);
    struct dirent de;

    kprintf("%s:", path);
    while (fd >= 0 && vfs_readdir(fd, &de) > 0) {
        kprintf(" %s%s(%u)", de.name, de.type == V_DIR ? "/" : "", de.size);
    }
    kprintf("\n");
    vfs_close(fd);
}

void init_main(void)
{
    char buf[64], cwd[PATH_MAX];
    int fd, n, total = 0, lines = 0, saved;

    kprintf("pipe: %d\n", vfs_pipe(fds));
    thread_create("writer", writer, NULL, 3, STACK_DEFAULT);
    while ((n = vfs_read(fds[0], buf, sizeof buf)) > 0) {
        int i;

        total += n;
        for (i = 0; i < n; i++) {
            lines += buf[i] == '\n';
        }
    }
    kprintf("reader: %d bytes, %d lines, then %d\n", total, lines, n);
    vfs_close(fds[0]);
    vfs_pipe(fds);
    vfs_close(fds[0]);
    n = vfs_write(fds[1], "x", 1);
    kprintf("write to closed pipe: %d\n", n);
    vfs_close(fds[1]);

    n = vfs_mkdir("/tmp/d");
    kprintf("mkdir /tmp/d: %d", n);
    n = vfs_mkdir("/tmp/d");
    kprintf(", again: %d", n);
    n = vfs_mkdir("/etc/x");
    kprintf(", in romfs: %d\n", n);
    fd = vfs_open("/tmp/d/f", O_WRONLY | O_CREAT);
    kprintf("create: %d\n", fd);
    for (n = 0; n < 30; n++) {
        vfs_write(fd, "0123456789", 10);   /* past the first buffer */
    }
    vfs_close(fd);
    fd = vfs_open("/tmp/d/f", O_WRONLY | O_APPEND);
    vfs_write(fd, "END", 3);
    vfs_close(fd);
    fd = vfs_open("/tmp/d/f", O_RDONLY);
    vfs_seek(fd, -13, SEEK_END);
    n = vfs_read(fd, buf, sizeof buf - 1);
    buf[n] = '\0';
    kprintf("tail: %s (%d)\n", buf, n);
    vfs_close(fd);
    kprintf("chdir: %d\n", vfs_chdir("/tmp/d"));
    vfs_getcwd(cwd, sizeof cwd);
    fd = vfs_open("f", O_RDONLY);
    kprintf("cwd %s, relative open %s, ../d/./f: %s, ../../etc/motd: %s\n", cwd,
            fd >= 0 ? "ok" : "no", vfs_open("../d/./f", O_RDONLY) >= 0 ? "ok" : "no",
            vfs_open("../../etc/motd", O_RDONLY) >= 0 ? "ok" : "no");
    fd = vfs_open("g", O_WRONLY | O_CREAT | O_TRUNC);
    vfs_write(fd, "small", 5);
    vfs_close(fd);
    fd = vfs_open("g", O_WRONLY | O_TRUNC);
    vfs_write(fd, "re", 2);
    vfs_close(fd);
    list("/tmp");
    list(".");
    kprintf("unlink dir with files: %d\n", vfs_unlink("/tmp/d"));
    n = vfs_unlink("f");
    kprintf("unlink f: %d", n);
    n = vfs_unlink("g");
    kprintf(", g: %d", n);
    n = vfs_unlink("g");
    kprintf(", gone: %d", n);
    n = vfs_unlink("/tmp/d");
    kprintf(", dir now: %d\n", n);
    list("/tmp");
    saved = vfs_dup(1);
    fd = vfs_open("/tmp/out", O_WRONLY | O_CREAT);
    vfs_dup2(fd, 1);
    vfs_close(fd);
    vfs_write(1, "this goes to the file\n", 22);
    vfs_dup2(saved, 1);
    vfs_close(saved);
    fd = vfs_open("/tmp/out", O_RDONLY);
    n = vfs_read(fd, buf, sizeof buf - 1);
    buf[n] = '\0';
    kprintf("captured: %s", buf);
    kernel_halt(0);
}
