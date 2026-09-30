/* For the tests: /tmp as a file system server, as /etc/rc starts it. */
static void start_memfs(void)
{
    char *args[] = { "memfs", "/tmp", NULL };
    const char *path, *type;
    int pid = process_spawn("/bin/memfs", 2, args), i, n;

    if (pid < 0) {
        kprintf("cannot start memfs: %d\n", pid);
        kernel_halt(1);
    }
    for (n = 0; n < 200; n++) {         /* until it has mounted */
        for (i = 0; vfs_mount_info(i, &path, &type) > 0; i++) {
            if (strcmp(path, "/tmp") == 0) {
                return;
            }
        }
        thread_sleep(1);
    }
    kpanic("memfs never mounted");
}
