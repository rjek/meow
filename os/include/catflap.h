/* Catflap's system calls, as a program sees them: the kernel's own entry
   points, reached by their addresses in ROM.  Values and layouts here
   must agree with os/kernel/kernel.h. */
#ifndef CATFLAP_H
#define CATFLAP_H

#define CF_O_RDONLY     0
#define CF_O_WRONLY     1
#define CF_O_RDWR       2
#define CF_O_CREAT      0x40
#define CF_O_TRUNC      0x200
#define CF_O_APPEND     0x400
#define CF_SEEK_SET     0
#define CF_SEEK_CUR     1
#define CF_SEEK_END     2

#define CF_V_FILE       0
#define CF_V_DIR        1
#define CF_V_DEV        2
#define CF_V_PIPE       3

struct cf_stat {
    int type;
    unsigned size;
    unsigned ino;
};

struct cf_dirent {
    char name[32];
    int type;
    unsigned size;
};

/* files: negative errno on failure */
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, unsigned len);
int vfs_write(int fd, const void *buf, unsigned len);
int vfs_seek(int fd, int off, int whence);
int vfs_readdir(int fd, struct cf_dirent *de);
int vfs_stat(const char *path, struct cf_stat *st);
int vfs_ioctl(int fd, int req, void *arg);
int vfs_dup(int fd);
int vfs_dup2(int fd, int to);
int vfs_mkdir(const char *path);
int vfs_unlink(const char *path);
int vfs_chdir(const char *path);
int vfs_getcwd(char *buf, unsigned size);
int vfs_pipe(int fds[2]);

/* processes */
int process_spawn(const char *path, int argc, char *const argv[]);
void process_exit(int status);
int process_wait(int pid, int *status);
int process_waitany(int *status, int block);   /* a pid, 0, or -ECHILD */
void *process_sbrk(int increment);
int process_pid(void);
struct cf_procinfo {
    int pid, parent, nthreads, dead;
    char name[32];
};
int process_info(int index, struct cf_procinfo *info);   /* 0 at the end */
unsigned kmem_free(void);

/* time */
unsigned ticks_now(void);               /* 100 a second since boot */
long kernel_time(void);                 /* seconds since 1970, from the host */
void thread_sleep(unsigned ticks);
void thread_yield(void);

#endif
