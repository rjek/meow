/* Stage 3: romfs, devfs and the VFS on top of them. */
#include "kernel.h"

static void list(const char *path)
{
    int fd = vfs_open(path, O_RDONLY);
    struct dirent de;

    kprintf("%s:", path);
    if (fd < 0) {
        kprintf(" error %d\n", fd);
        return;
    }
    while (vfs_readdir(fd, &de) > 0) {
        kprintf(" %s%s", de.name, de.type == V_DIR ? "/" : "");
    }
    kprintf("\n");
    vfs_close(fd);
}

static int cfd;                         /* the console */

static void cat(const char *path)
{
    char buf[32];
    int fd = vfs_open(path, O_RDONLY), n;

    if (fd < 0) {
        kprintf("%s: error %d\n", path, fd);
        return;
    }
    while ((n = vfs_read(fd, buf, sizeof buf)) > 0) {
        vfs_write(cfd, buf, (size_t)n);
    }
    vfs_close(fd);
}

void init_main(void)
{
    struct stat st;
    char line[64];
    int fd, n;

    cfd = vfs_open("/dev/console", O_RDWR);
    kprintf("console is fd %d\n", cfd);
    list("/");
    list("/etc");
    list("/dev");
    list("/nowhere");
    cat("/etc/motd");
    cat("/etc/config");
    kprintf("stat /etc/motd: %d type %d size %u\n", vfs_stat("/etc/motd", &st), st.type, st.size);
    kprintf("stat /etc: %d type %d\n", vfs_stat("/etc", &st), st.type);
    kprintf("open /etc/motd/x: %d, open /bin/nothing: %d, read dir: %d\n",
            vfs_open("/etc/motd/x", O_RDONLY), vfs_open("/bin/nothing", O_RDONLY),
            (fd = vfs_open("/etc", O_RDONLY), n = vfs_read(fd, line, 8), vfs_close(fd), n));
    fd = vfs_open("/etc/motd", O_RDONLY);
    vfs_seek(fd, -5, SEEK_END);
    n = vfs_read(fd, line, sizeof line);
    line[n] = '\0';
    kprintf("tail: [%s] %d\n", line, n);
    vfs_close(fd);
    while ((n = vfs_read(cfd, line, sizeof line - 1)) > 0) {
        line[n] = '\0';
        kprintf("read %d: %s", n, line);
    }
    kprintf("end of console input\n");
    kprintf("%s free\n", kmem_free() > 200 * 1024 ? "plenty" : "little");
    kernel_halt(0);
}
