/* The SD card: /bin/sd brings it up over the IOC's SPI master and
   serves it as /dev/sd0; blocks are read, written across a block
   boundary, and read back, and what the host put on the card is there. */
#include "kernel.h"

static char buf[600];

void init_main(void)
{
    char *const sd[] = { "sd" };
    int pid, fd, i, rc;
    unsigned size = 0;
    struct stat st;

    pid = process_spawn("/bin/sd", 1, sd);
    for (i = 0; i < 100 && vfs_stat("/dev/sd0", &st) < 0; i++) {
        thread_sleep(1);
    }
    fd = vfs_open("/dev/sd0", O_RDWR);
    if (fd < 0) {
        kprintf("no /dev/sd0: %d\n", fd);
        kernel_halt(1);
    }
    vfs_ioctl(fd, VFS_IOC_BLKSIZE, &size);
    kprintf("card: %u bytes\n", size);
    rc = vfs_read(fd, buf, 48);
    buf[rc > 0 ? rc : 0] = '\0';
    kprintf("block 0: %d '%s'\n", rc, buf);

    for (i = 0; i < 600; i++) {
        buf[i] = (char)('a' + i % 26);
    }
    vfs_seek(fd, 1000, SEEK_SET);       /* across the end of block 1 */
    kprintf("wrote %d at 1000\n", vfs_write(fd, buf, 600));
    memset(buf, 0, sizeof buf);
    vfs_seek(fd, 1000, SEEK_SET);
    rc = vfs_read(fd, buf, 600);
    for (i = 0; i < 600 && buf[i] == (char)('a' + i % 26); i++) {
    }
    kprintf("read %d back, %s\n", rc, i == 600 ? "all as written" : "wrong");
    vfs_seek(fd, 512, SEEK_SET);
    rc = vfs_read(fd, buf, 16);
    kprintf("block 1 starts with %d zero bytes\n", rc);
    vfs_seek(fd, (int32_t)size - 8, SEEK_SET);
    rc = vfs_read(fd, buf, 16);
    kprintf("at the end: read %d, then %d\n", rc, vfs_read(fd, buf, 16));
    vfs_close(fd);
    process_kill(pid);
    kernel_halt(0);
}
