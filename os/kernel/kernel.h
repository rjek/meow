/* Catflap: what the kernel's parts know about each other. */
#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* The machine */
#define RAM_BASE        0x08000000u
#define CHAIRMAN        0xF8000000u
#define CH_REG(off)     (*(volatile uint32_t *)(CHAIRMAN + (off)))
#define CH_CS_DEVICE(n) CH_REG(256 * (n))
#define CH_CS_SIZE(n)   CH_REG(256 * (n) + 4)
#define DEV_IOC         0x00000003u
#define CH_MASK(cpu)    CH_REG(0x2000 + 4 * (cpu))
#define CH_PENDING      CH_REG(0x2400)
#define CH_TIMER_HZ     CH_REG(0x2404)
#define CH_TIMER_RELOAD CH_REG(0x2408)
#define CH_TIMER_VALUE  CH_REG(0x240c)
#define CH_CPU_STATUS(n) CH_REG(0x2800 + 0x20 * (n))
#define CH_CPU_START(n) CH_REG(0x2804 + 0x20 * (n))
#define CH_CPU_CONTROL(n) CH_REG(0x2808 + 0x20 * (n))
#define CH_DOORBELL(n)  CH_REG(0x280c + 0x20 * (n))
#define CH_CPU_TIMER_RELOAD(n) CH_REG(0x2810 + 0x20 * (n))
#define CH_PRESENT      CH_REG(0x2c00)
#define CH_LOCK(n)      CH_REG(0x2e00 + 4 * (n))
#define IRQ_UART0       0               /* the IOC's sources are 0 to 7 */
#define IRQ_DOORBELL    30
#define IRQ_TIMER       31
#define LOCAL_BASE      0xF0000000u     /* this CPU's local memory */
#define LOCAL_ALL       0xE8000000u     /* every CPU's, 4 MB apart */
#define LOCAL_STRIDE    0x00400000u
#define NCPU            32
#define BOOT_STACK      4096            /* top of RAM: the boot thread's, then idle's */
#define IRQ_STACK       4096            /* below it: the interrupt bank's; boot.s agrees */
#define HZ              100
#define SLICE           2               /* ticks a thread runs before an equal takes over */

/* Threads */
#define NPRIO           8
#define PRIO_IDLE       0
#define PRIO_DEFAULT    4
#define PRIO_INIT       7
#define PRIO_USER_MAX   6               /* a program's threads stay below init's */
#define STACK_DEFAULT   1024
#define STACK_USER      4096            /* a program's main thread, unless it says */
#define STACK_MAGIC     0x57ac6ed5u     /* the lowest word of every thread stack */

enum thread_state { T_READY, T_RUNNING, T_SLEEPING, T_BLOCKED, T_ZOMBIE };

struct process;
struct request;

struct waitq {
    struct thread *head, *tail;
};

struct thread {
    uint32_t regs[16];                  /* r0 to pc, saved by boot.s: keep first */
    struct thread *next;                /* the run, sleep or zombie list */
    struct thread *all;                 /* every thread there is */
    struct process *proc;
    struct waitq *waiting;              /* what it is blocked on */
    enum thread_state state;
    int prio;
    int cpu;                            /* the CPU it runs on, always */
    int in_kernel;                      /* the CPU's counter while switched out */
    uint32_t wake;                      /* tick to wake at, when sleeping */
    void *stack;
    size_t stack_size;
    int local;                          /* the stack is its CPU's local memory, not the heap's */
    const char *name;
    int exit_status;
    int tid;
    struct thread *tnext;               /* those waiting with a time limit */
    int timed;                          /* it is one of them */
    int timedout;                       /* and the limit is what woke it */
    struct request *req;                /* what it is asking a server, if anything */
    int doomed;                         /* to end at its next kernel exit, and its process too if that is still to do */
};

#define CPU_EXCLUSIVE   0x100           /* a cpu argument's flag: the thread has the CPU to itself */
#define LOCAL_CODE      256             /* where a local thread's code goes in local memory, past struct cpu */
#define LOCAL_MIN_STACK 256

#define R_SP 11
#define R_LR 12
#define R_SR 14
#define R_PC 15

#define CATFLAP_VERSION "0.7"

/* Per-CPU state, at the start of each CPU's local memory, so that a CPU
   finds its own at one address (LOCAL_BASE) and another's through
   LOCAL_ALL.  The first five words are boot.s's: keep them first. */
struct cpu {
    struct thread *switch_from;         /* the switch boot.s is to make */
    struct thread *switch_to;
    uint32_t client_sb;                 /* __client_sb: the running process's displacement */
    uint32_t boot_sp;                   /* a CPU starting: its idle stack */
    uint32_t irq_sp;                    /* and the interrupt bank's */
    int cpu;
    volatile int online;                /* set by the CPU itself, awaited by CPU 0 */
    struct thread *current;
    struct thread *idle;
    struct thread *exclusive;           /* the thread that has the CPU to itself, if any */
    int in_kernel;                      /* > 0: inside a kernel call, do not switch */
    volatile int switch_wanted;         /* kernel code asked for a switch, and spins on it */
    int tick_pending;                   /* ticks not yet accounted for */
    int poked;                          /* the doorbell rang: something to reconsider */
    uint32_t devices;                   /* device sources rung, masked off until handled */
    int slice;                          /* ticks left in the current one */
    struct thread *ready_head[NPRIO], *ready_tail[NPRIO];
};

#define this_cpu() ((struct cpu *)LOCAL_BASE)
#define cpu_of(n) ((struct cpu *)(LOCAL_ALL + LOCAL_STRIDE * (unsigned)(n)))

/* Errors: negative errno values, as the system calls return them */
#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
#define EBADF           9
#define ENOMEM          12
#define EBUSY           16
#define EEXIST          17
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define EMFILE          24
#define ENOTTY          25
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define ENAMETOOLONG    36
#define ENOSYS          38
#define ENOTEMPTY       39
#define ENOEXEC         8
#define ECHILD          10
#define EPIPE           32
#define EDEADLK         35
#define ETIMEDOUT       110

/* The file system */
#define NFD             16
#define NAME_MAX        31
#define PATH_MAX        128
#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_CREAT         0x40
#define O_TRUNC         0x200
#define O_APPEND        0x400
#define VFS_IOC_ADDR    0x5601          /* ioctl: *(const void **)arg is the file's bytes, if mapped */
#define VFS_IOC_BLKSIZE 0x4a01          /* ioctl on a block device: *(unsigned *)arg is its size in bytes */
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

enum vnode_type { V_FILE, V_DIR, V_DEV, V_PIPE, V_MQ, V_SEM, V_PORT };

/* what poll reports, with POSIX's values */
#define POLLIN          1
#define POLLOUT         4
#define POLLERR         8
#define POLLHUP         16
#define POLLNVAL        32

struct pollfd {
    int fd;
    short events, revents;
};

struct vnode;
struct dirent {
    char name[NAME_MAX + 1];
    int type;
    uint32_t size;
};

struct stat {
    int type;
    uint32_t size;
    uint32_t ino;
};

struct vnode_ops {
    int (*lookup)(struct vnode *dir, const char *name, struct vnode **out);
    int (*read)(struct vnode *v, void *buf, size_t len, uint32_t off);
    int (*write)(struct vnode *v, const void *buf, size_t len, uint32_t off);
    int (*readdir)(struct vnode *v, uint32_t index, struct dirent *out);
    int (*create)(struct vnode *dir, const char *name, int type, struct vnode **out);
    int (*unlink)(struct vnode *dir, const char *name);
    int (*ioctl)(struct vnode *v, int req, void *arg);
    void (*release)(struct vnode *v);
    int (*truncate)(struct vnode *v);
    int (*poll)(struct vnode *v);       /* POLLIN and so on as they stand; NULL: always ready */
};

struct vnode {
    const struct vnode_ops *ops;
    int type;
    uint32_t size;
    int refs;
    void *fs;                           /* the file system's own */
    uint32_t ino;
};

struct file {
    struct vnode *v;
    uint32_t off;
    int flags;
    int refs;
    char *path;                         /* a directory's, for listing the mounts in it */
    uint32_t fs_entries;                /* how many its file system listed, once known */
    int fs_done;
};

/* process.c */
struct heapblk {
    struct heapblk *next;
    size_t size, used;                  /* bytes after this header */
};

struct process {
    int pid;
    const char *name;
    struct process *next;
    struct process *parent;
    struct thread *main;
    int nthreads;
    int dead;
    int killed;                         /* the status to end with, once told to */
    int exit_status;
    char *image;                        /* the code's copy in RAM, or NULL when run in place */
    const char *code;                   /* where the code runs */
    size_t code_size;
    size_t data_size;                   /* the data block: the library's, then the program's */
    uint32_t entry;
    void *libdata;                      /* this process's copy of the library's data */
    uint32_t sb;                        /* its displacement from the linked copy */
    int argc;
    char **argv;
    struct heapblk *heap;               /* the program's, newest first, handed out by sbrk */
    int orphan;                         /* its parent ended first: nobody will wait */
    size_t stack_size;
    struct file *fds[NFD];
    char cwd[PATH_MAX];
    struct waitq waiters;               /* for this one to end */
    struct waitq childq;                /* for any of its children to end */
};

extern struct process kproc;
struct process *current_process(void);
struct process *process_find(int pid);
void process_init(const uint32_t *relocs, uint32_t n);
int process_spawn(const char *path, int argc, char *const argv[]);
int process_spawn_on(const char *path, int argc, char *const argv[], int cpu);
void process_exit(int status);
int process_wait(int pid, int *status);
void process_thread_gone(struct process *p);
void *process_sbrk(int increment);
int process_pid(void);
struct process *process_list(void);
#define HEAP_CHUNK      (32 * 1024)     /* the least sbrk takes from the kernel at a time */
int process_waitany(int *status, int block);
int process_kill(int pid);
#define KILLED          137             /* the exit status of a process that was */
void reap_orphans(void);
/* vfs.c */
struct vnode *vnode_new(const struct vnode_ops *ops, int type, void *fs,
                        uint32_t ino, uint32_t size);
void vnode_get(struct vnode *v);
void vnode_put(struct vnode *v);
int vfs_mount(const char *path, struct vnode *root, const char *type);
int vfs_mount_owned(const char *path, struct vnode *root, const char *type, void *owner);
void vfs_umount_owner(void *owner);
struct vnode *vfs_fd_vnode(int fd);
int vfs_poll(struct pollfd *p, unsigned n, int ticks);
struct thread *poll_wake(void);
int vfs_mount_info(int index, const char **path, const char **type);
int vfs_lookup(const char *path, struct vnode **out);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, size_t len);
int vfs_write(int fd, const void *buf, size_t len);
int vfs_seek(int fd, int32_t off, int whence);
int vfs_readdir(int fd, struct dirent *de);
int vfs_stat(const char *path, struct stat *st);
int vfs_ioctl(int fd, int req, void *arg);
int vfs_open_vnode(struct vnode *v, int flags);
int vfs_dup(int fd);
int vfs_dup2(int fd, int to);
int vfs_mkdir(const char *path);
int vfs_unlink(const char *path);
int vfs_chdir(const char *path);
int vfs_getcwd(char *buf, size_t size);
int vfs_pipe(int fds[2]);
void pipe_end_closed(struct vnode *v, int flags);

/* romfs.c, devfs.c */
struct vnode *romfs_init(const void *image);
struct vnode *devfs_init(void);
struct vnode *hostfs_init(void);
struct vnode *procfs_init(void);
struct vnode *ipcfs_init(void);

/* ipcfs.c: named message queues and semaphores in /ipc */
#define IPC_MQ          1
#define IPC_SEM         2
#define IPC_TRYSEND     0x4901          /* ioctl on a queue: arg the message; 1 sent, 0 full */
#define IPC_TRYRECV     0x4902          /* ioctl on a queue: arg the buffer; 1 received, 0 empty */
#define IPC_TRYWAIT     0x4903          /* ioctl on a semaphore: 1 taken, 0 not */
#define IPC_VALUE       0x4904          /* ioctl on either: messages waiting, or the count */
int ipc_create(const char *name, int kind, int a, int b);
int dev_register(const char *name, const struct vnode_ops *ops, void *ctx);
int dev_register_served(const char *name, void *port, uint32_t node);
void dev_unregister_owner(void *port);

/* srv.c: ports, through which a program serves device nodes and file
   systems.  A request is what the server is handed: the operation, the
   server's own number for the file, and the client's buffer, which the
   server reads or writes where it lies.  catflap.h has the same layout
   as struct cf_req. */
enum { SRV_LOOKUP = 1, SRV_READ, SRV_WRITE, SRV_READDIR, SRV_CREATE,
       SRV_UNLINK, SRV_IOCTL, SRV_TRUNCATE, SRV_RELEASE };
#define SRV_WANT_RELEASE 1              /* srv_create flag: send SRV_RELEASE */

struct srv_req {
    int op;
    uint32_t node;                      /* the file, or the directory to look in */
    void *buf;                          /* data, a name, a struct dirent, ioctl's argument */
    uint32_t len;
    uint32_t off;                       /* the offset; readdir's index; ioctl's request; create's type */
    int pid;                            /* who is asking */
    int cancelled;                      /* delivered again: the client is going, answer now */
    uint32_t new_node;                  /* answers to lookup and create */
    int type;
    uint32_t size;                      /* the file's size: as the kernel has it, and as it is now */
};

int srv_create(int timeout, int flags);
int srv_dev(int port, const char *name, uint32_t node);
int srv_mount(int port, const char *path, uint32_t root);
int srv_recv(int port, struct srv_req **req, int ticks);
int srv_reply(int port, struct srv_req *req, int result);
int srv_ready(int port, uint32_t node, int mask);
struct vnode *srv_vnode(void *port, uint32_t node, int type, uint32_t size);
int srv_abandon(struct thread *t);
int srv_holds(struct thread *t);

/* boot.s */
void cpu_halt(int status);
void kernel_halt(int status);           /* lib.c: the console flushed, then cpu_halt */
int cpu_id(void);
long kernel_time(void);
int host_call(int op, int a, int b, int c, int d);
int cpu_model(void);                    /* BNV #0's model byte: 0 msim, 1 MEOW1 */
void cpu_wfi(void);                     /* BNV #6: wait for an interrupt */
void cpu_entry(void);                   /* where a CPU other than 0 starts */

/* lib.c: the kernel's own, under the usual names here but not clashing
   with the C library's in the same image */
#define memcpy k_memcpy
#define memset k_memset
#define memcmp k_memcmp
#define strlen k_strlen
#define strcmp k_strcmp
#define strncmp k_strncmp
#define strcpy k_strcpy
#define strcat k_strcat
#define strchr k_strchr
#define strrchr k_strrchr
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int memcmp(const void *a, const void *b, size_t n);
char *strcpy(char *d, const char *s);
char *strcat(char *d, const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
void strncpy_(char *d, const char *s, size_t size);   /* always terminated */

void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);
void kpanic(const char *fmt, ...);

/* sync.c */
void waitq_remove(struct waitq *q, struct thread *t);

struct sem {
    int count;
    struct waitq q;
};

struct mutex {
    struct thread *owner;
    int owner_prio;                     /* to restore after inheritance */
    struct waitq q;
};

struct mq {
    char *buf;
    size_t msgsize;
    unsigned depth, head, count;
    struct waitq readers, writers;
};

void waitq_init(struct waitq *q);
void waitq_wait(struct waitq *q);
int waitq_wait_for(struct waitq *q, uint32_t ticks);
void wait_forget(struct thread *t);
int wait_expire(uint32_t now);
struct thread *waitq_wake_one(struct waitq *q);
void preempt_if(struct thread *t);
void sem_init(struct sem *s, int count);
void sem_wait(struct sem *s);
int sem_trywait(struct sem *s);
void sem_post(struct sem *s);
void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);
int mq_init(struct mq *q, size_t msgsize, unsigned depth);
void mq_destroy(struct mq *q);
int mq_send(struct mq *q, const void *msg, int block);
int mq_receive(struct mq *q, void *msg, int block);

/* console.c */
void console_init(void);
void console_start(void);
void console_flush(void);
void console_putc(int c);
void console_puts(const char *s);
void console_out(const char *s, size_t n);     /* in one piece */
int console_getc(void);
int console_pending(void);
int console_readable(void);
int console_gets(char *buf, size_t size);

/* alloc.c */
void alloc_init(void *base, void *limit);
void *kmalloc(size_t n);
void kfree(void *p);
size_t kmem_free(void);

/* sched.c */
void sched_init(void);
void sched_start(void);
struct thread *thread_create(const char *name, int (*fn)(void *), void *arg,
                             int prio, size_t stack_size);
struct thread *thread_create_on(const char *name, int (*fn)(void *), void *arg,
                                int prio, size_t stack_size, int cpu);
struct thread *thread_create_in(struct process *p, const char *name,
                                int (*fn)(void *), void *arg, int prio,
                                size_t stack_size, int cpu);
struct thread *thread_create_local(struct process *p, const char *name,
                                   int (*fn)(void *), size_t code_size,
                                   void *arg, int prio, int cpu);
int thread_spawn_local(int (*fn)(void *), unsigned code_size, void *arg, int prio, int cpu);
void thread_kill_others(struct process *p);
void thread_kill_process(struct process *p);
int thread_spawn(int (*fn)(void *), void *arg, unsigned stack, int prio);
int thread_spawn_on(int (*fn)(void *), void *arg, unsigned stack, int prio, int cpu);
int cpu_count(void);
int cpu_start(int n);
void cpu_main(void);
int thread_id(void);
struct thread *thread_list(void);
void thread_exit(int status);
void thread_yield(void);
void thread_sleep(uint32_t ticks);
struct thread *thread_current(void);
uint32_t ticks_now(void);
void kenter(void);
void kexit(void);
void schedule(void);
void thread_ready(struct thread *t);
void thread_block(void);
void idle_work(void);
void reap_zombies(void);
void irq_dispatch(void);
/* A device's interrupt handler: runs with the kernel's data consistent,
   on the CPU whose mask has the source, and returns a thread it woke,
   if any, for the scheduler to consider */
void irq_attach(int source, struct thread *(*handler)(void));

/* whoever provides init_main: init.c, or a test */
void init_main(void);

#endif
