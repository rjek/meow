/* A FAT volume on the SD card, served by /bin/fatfs: files the host put
   there are read and listed; files are made, written across clusters,
   read back, grown, truncated and removed; directories are made and
   removed; and the names and errors come out right. */
#include "kernel.h"

static char buf[2048];

static void list(const char *path)
{
    int fd = vfs_open(path, O_RDONLY);
    struct dirent de;

    kprintf("%s:", path);
    while (fd >= 0 && vfs_readdir(fd, &de) > 0) {
        kprintf(" %s%s(%u)", de.name, de.type == V_DIR ? "/" : "", (unsigned)de.size);
    }
    kprintf("\n");
    vfs_close(fd);
}

static int cat(const char *path, int show)
{
    int fd = vfs_open(path, O_RDONLY), n, total = 0;

    if (fd < 0) {
        return fd;
    }
    while ((n = vfs_read(fd, buf, sizeof buf - 1)) > 0) {
        total += n;
        if (show != 0) {
            buf[n] = '\0';
            kprintf("%s", buf);
        }
    }
    vfs_close(fd);
    return total;
}

void init_main(void)
{
    char *const sd[] = { "sd" };
    char *const fatfs[] = { "fatfs", "/dev/sd0", "/sd" };
    struct stat st;
    int i, fd, n;

    process_spawn("/bin/sd", 1, sd);
    process_spawn("/bin/fatfs", 3, fatfs);
    for (i = 0; i < 200 && vfs_stat("/sd/hello.txt", &st) < 0; i++) {
        thread_sleep(1);
    }
    kprintf("hello.txt: %d bytes, type %d\n", (int)st.size, st.type);
    cat("/sd/hello.txt", 1);
    list("/sd");
    n = cat("/sd/big.txt", 0);
    fd = vfs_open("/sd/big.txt", O_RDONLY);
    vfs_seek(fd, 47 * 150, SEEK_SET);   /* a line in the middle, across a sector */
    vfs_read(fd, buf, 47);
    buf[46] = '\0';
    vfs_close(fd);
    kprintf("big.txt: %d bytes; line 150 is '%s'\n", n, buf);
    kprintf("missing: %d, bad name: %d\n", vfs_open("/sd/nothere.txt", O_RDONLY),
            vfs_open("/sd/a_name_too_long.txt", O_WRONLY | O_CREAT));

    for (i = 0; i < (int)sizeof buf; i++) {
        buf[i] = (char)('A' + i % 23);
    }
    fd = vfs_open("/sd/new.txt", O_WRONLY | O_CREAT);
    kprintf("new.txt: wrote %d", vfs_write(fd, buf, 1500));
    kprintf(" and %d", vfs_write(fd, buf, 600));
    vfs_close(fd);
    vfs_stat("/sd/new.txt", &st);
    kprintf(", size %u\n", (unsigned)st.size);
    fd = vfs_open("/sd/new.txt", O_RDONLY);
    n = vfs_read(fd, buf, sizeof buf);
    for (i = 0; i < 1500 && buf[i] == (char)('A' + i % 23); i++) {
    }
    kprintf("read %d back, first 1500 %s", n, i == 1500 ? "right" : "wrong");
    for (i = 1500; i < n && buf[i] == (char)('A' + (i - 1500) % 23); i++) {
    }
    kprintf(", the rest %s\n", i == n ? "right" : "wrong");
    vfs_close(fd);
    fd = vfs_open("/sd/new.txt", O_WRONLY | O_TRUNC);
    vfs_write(fd, "short", 5);
    vfs_close(fd);
    kprintf("after truncation: %d bytes\n", cat("/sd/new.txt", 0));

    kprintf("mkdir: %d", vfs_mkdir("/sd/dir"));
    kprintf(", again: %d\n", vfs_mkdir("/sd/dir"));
    fd = vfs_open("/sd/dir/inner.txt", O_WRONLY | O_CREAT);
    vfs_write(fd, "inside\n", 7);
    vfs_close(fd);
    list("/sd/dir");
    list("/sd");
    kprintf("rmdir full: %d", vfs_unlink("/sd/dir"));
    kprintf(", inner: %d", vfs_unlink("/sd/dir/inner.txt"));
    kprintf(", rmdir: %d", vfs_unlink("/sd/dir"));
    kprintf(", hello: %d\n", vfs_unlink("/sd/hello.txt"));
    list("/sd");

    for (i = 0; i < 20; i++) {          /* a file past the first cluster of the FAT's reach */
        fd = vfs_open("/sd/new.txt", O_WRONLY | O_APPEND);
        vfs_write(fd, buf, 1024);
        vfs_close(fd);
    }
    kprintf("new.txt grew to %d\n", cat("/sd/new.txt", 0));
    kernel_halt(0);
}
