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
#define CF_V_PORT       6

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

/* Which of n descriptors can be read or written without waiting: sets
   revents in each and returns how many have any.  Waits up to ticks for
   the first; 0 does not wait and a negative number waits for ever. */
#define CF_POLLIN       1
#define CF_POLLOUT      4
#define CF_POLLERR      8               /* reported whether asked for or not */
#define CF_POLLHUP      16
#define CF_POLLNVAL     32

struct cf_pollfd {
    int fd;
    short events, revents;
};

int vfs_poll(struct cf_pollfd *fds, unsigned n, int ticks);

/* processes */
int process_spawn(const char *path, int argc, char *const argv[]);
void process_exit(int status);
int process_wait(int pid, int *status);
int process_waitany(int *status, int block);   /* a pid, 0, or -ECHILD */
int process_kill(int pid);              /* it exits with CF_KILLED */
#define CF_KILLED       137
void *process_sbrk(int increment);
int process_pid(void);

/* threads within the program: fn(arg) runs until it returns.  stack 0
   means 4 KB; prio 1 to 6, anything else the default of 4.  The C
   library keeps no locks: only one thread at a time may use stdio or
   malloc. */
int thread_spawn(int (*fn)(void *), void *arg, unsigned stack, int prio);
int thread_id(void);

/* CPUs: a thread runs on the CPU it was made for, which is its maker's
   unless it says.  With CF_CPU_EXCLUSIVE the thread has the CPU to
   itself: no other thread is put there while it lives and its tick is
   stopped, so nothing interrupts it; CPU 0 cannot be had this way.
   Kernel calls from such a thread still work, but each may wait for
   the kernel to be free. */
#define CF_CPU_EXCLUSIVE 0x100
int cpu_count(void);
int cpu_id(void);
int thread_spawn_on(int (*fn)(void *), void *arg, unsigned stack, int prio, int cpu);
int process_spawn_on(const char *path, int argc, char *const argv[], int cpu);

/* An exclusive thread with its stack in its CPU's local memory (the
   reference's chip select 30) and, if code_size is not 0, that many
   bytes of code from fn copied there too, so that it runs touching
   nothing on the shared bus but what it chooses: a peripheral written
   in C.  The code must stand being moved: branches and literal loads
   are relative, so a function and the literal pool the compiler puts
   after it move as one, and the size to give is the distance to the
   next function in the file, (char *)next - (char *)fn.  Calls and
   static data are reached by absolute address and stay where they are.
   The stack is what is left of the local memory. */
int thread_spawn_local(int (*fn)(void *), unsigned code_size, void *arg, int prio, int cpu);

/* The Chairman's chip-select table, for finding the IOC and the rest:
   entry n's first word is the device, vendor in the top half, number in
   the bottom, and 0xffffffff for nothing there. */
#define CF_CHAIRMAN     0xF8000000u
#define CF_CS_DEVICE(n) (*(volatile unsigned *)(CF_CHAIRMAN + 256u * (n)))
#define CF_CS_BASE(n)   ((n) << 27)
#define CF_DEV_IOC      0x00000003u

/* named IPC in /ipc: open the name after creating it.  A queue's write
   sends exactly one message of its size and its read takes one; a
   semaphore's read waits and its write posts.  Both block. */
#define CF_IPC_MQ       1               /* a is the message size, b the depth */
#define CF_IPC_SEM      2               /* a is the starting count */
#define CF_IPC_TRYSEND  0x4901          /* vfs_ioctl(fd, ., msg): 1 sent, 0 full */
#define CF_IPC_TRYRECV  0x4902          /* vfs_ioctl(fd, ., buf): 1 received, 0 empty */
#define CF_IPC_TRYWAIT  0x4903          /* vfs_ioctl(fd, ., 0): 1 taken, 0 not */
#define CF_IPC_VALUE    0x4904          /* vfs_ioctl(fd, ., 0): messages waiting, or the count */
#define CF_BLK_SIZE     0x4a01          /* vfs_ioctl(fd, ., &unsigned) on a block device: its size in bytes */
int ipc_create(const char *name, int kind, int a, int b);

/* Serving files.  A port is a descriptor.  srv_dev() makes /dev/NAME and
   srv_mount() a file system at a path; after that every operation on
   them by any other program arrives at srv_recv() as a request, and the
   caller waits until srv_reply() gives it its result: a count or 0, or
   a negative errno.  The port's nodes and mounts go when it is closed.

   A request's buf is the caller's own buffer: read from it or write to
   it directly.  It and the request stay valid until the reply, and not
   a moment longer.  A request need not be answered before the next is
   taken: a read with nothing to give can be kept until there is.  One
   that comes round again with `cancelled` set must be answered at
   once, with anything; its caller is being killed and cannot go until
   it is.

   node is the server's own number for a file, whatever it likes:
      op            node        buf, len        off       also
      CF_OP_LOOKUP  directory   name            -         answer in new_node, type, size
      CF_OP_READ    file        where to put    offset    result: bytes read
      CF_OP_WRITE   file        what to write   offset    result: bytes taken; size if it changed
      CF_OP_READDIR directory   a cf_dirent     index     result: 1, or 0 at the end
      CF_OP_CREATE  directory   name            type      answer in new_node
      CF_OP_UNLINK  directory   name            -
      CF_OP_IOCTL   file        the argument    request
      CF_OP_TRUNCATE file       -               -         size is now 0
      CF_OP_RELEASE file        -               -         only with CF_SRV_RELEASE: a use of it ended */
enum { CF_OP_LOOKUP = 1, CF_OP_READ, CF_OP_WRITE, CF_OP_READDIR, CF_OP_CREATE,
       CF_OP_UNLINK, CF_OP_IOCTL, CF_OP_TRUNCATE, CF_OP_RELEASE };
#define CF_SRV_RELEASE  1

struct cf_req {
    int op;
    unsigned node;
    void *buf;
    unsigned len;
    unsigned off;
    int pid;                            /* who is asking */
    int cancelled;
    unsigned new_node;
    int type;                           /* CF_V_FILE or CF_V_DIR */
    unsigned size;
};

/* timeout: the ticks the server may stay away from srv_recv() while
   requests wait before it is killed and they fail, or 0 for no limit */
int srv_create(int timeout, int flags);
int srv_dev(int port, const char *name, unsigned node);
int srv_mount(int port, const char *path, unsigned root);
int srv_recv(int port, struct cf_req **req, int ticks);        /* 1, or 0 when ticks ran out */
int srv_reply(int port, struct cf_req *req, int result);
int srv_ready(int port, unsigned node, int mask);              /* what poll says of a node */

/* time */
unsigned ticks_now(void);               /* 100 a second since boot */
long kernel_time(void);                 /* seconds since 1970, from the host */
void thread_sleep(unsigned ticks);
void thread_yield(void);

#endif
