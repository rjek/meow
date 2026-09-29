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
#define CF_V_MQ         4
#define CF_V_SEM        5

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

/* threads within the program: fn(arg) runs until it returns.  stack 0
   means 4 KB; prio 1 to 6, anything else the default of 4.  The C
   library keeps no locks: only one thread at a time may use stdio or
   malloc. */
int thread_spawn(int (*fn)(void *), void *arg, unsigned stack, int prio);
int thread_id(void);

/* named IPC in /ipc: open the name after creating it.  A queue's write
   sends exactly one message of its size and its read takes one; a
   semaphore's read waits and its write posts.  Both block. */
#define CF_IPC_MQ       1               /* a is the message size, b the depth */
#define CF_IPC_SEM      2               /* a is the starting count */
#define CF_IPC_TRYSEND  0x4901          /* vfs_ioctl(fd, ., msg): 1 sent, 0 full */
#define CF_IPC_TRYRECV  0x4902          /* vfs_ioctl(fd, ., buf): 1 received, 0 empty */
#define CF_IPC_TRYWAIT  0x4903          /* vfs_ioctl(fd, ., 0): 1 taken, 0 not */
#define CF_IPC_VALUE    0x4904          /* vfs_ioctl(fd, ., 0): messages waiting, or the count */
int ipc_create(const char *name, int kind, int a, int b);

/* time */
unsigned ticks_now(void);               /* 100 a second since boot */
long kernel_time(void);                 /* seconds since 1970, from the host */
void thread_sleep(unsigned ticks);
void thread_yield(void);

#endif
