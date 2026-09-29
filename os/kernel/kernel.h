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
#define CH_CS_SIZE(n)   CH_REG(256 * (n) + 4)
#define CH_MASK(cpu)    CH_REG(0x2000 + 4 * (cpu))
#define CH_PENDING      CH_REG(0x2400)
#define CH_TIMER_HZ     CH_REG(0x2404)
#define CH_TIMER_RELOAD CH_REG(0x2408)
#define CH_TIMER_VALUE  CH_REG(0x240c)
#define CH_SERIAL_FLAGS CH_REG(0x2410)
#define CH_SERIAL_IN    CH_REG(0x2414)
#define CH_SERIAL_OUT   CH_REG(0x2418)
#define IRQ_TIMER       31
#define BOOT_STACK      4096            /* top of RAM: the boot thread's, then idle's */
#define IRQ_STACK       4096            /* below it: the interrupt bank's; boot.s agrees */
#define HZ              100
#define SLICE           2               /* ticks a thread runs before an equal takes over */

/* Threads */
#define NPRIO           8
#define PRIO_IDLE       0
#define PRIO_DEFAULT    4
#define PRIO_INIT       7
#define STACK_DEFAULT   1024
#define STACK_USER      4096            /* a program's main thread */

enum thread_state { T_READY, T_RUNNING, T_SLEEPING, T_BLOCKED, T_ZOMBIE };

struct process;

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
    int in_kernel;                      /* the CPU's counter while switched out */
    uint32_t wake;                      /* tick to wake at, when sleeping */
    void *stack;
    size_t stack_size;
    const char *name;
    int exit_status;
};

#define R_SP 11
#define R_LR 12
#define R_SR 14
#define R_PC 15

/* Per-CPU state.  One CPU today; the layout is what a second would
   index by cpu_id(). */
struct cpu {
    struct thread *current;
    int in_kernel;                      /* > 0: inside a kernel call, do not switch */
    int switch_wanted;                  /* kernel code asked for a switch */
    int tick_pending;                   /* ticks not yet accounted for */
    int slice;                          /* ticks left in the current one */
};

extern struct cpu cpu0;
#define this_cpu() (&cpu0)

/* Errors: negative errno values, as the system calls return them */
#define ENOENT          2
#define EIO             5
#define EBADF           9
#define ENOMEM          12
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
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

enum vnode_type { V_FILE, V_DIR, V_DEV, V_PIPE };

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
};

/* process.c */
struct process {
    int pid;
    const char *name;
    struct process *next;
    struct process *parent;
    struct thread *main;
    int nthreads;
    int dead;
    int exit_status;
    char *image;
    size_t image_size;
    uint32_t entry;
    void *libdata;                      /* this process's copy of the library's data */
    uint32_t sb;                        /* its displacement from the linked copy */
    int argc;
    char **argv;
    char *heap;                         /* the program's, handed out by sbrk */
    size_t heap_size;
    size_t brk;
    struct file *fds[NFD];
    struct waitq waiters;
};

extern struct process kproc;
extern uint32_t __client_sb;
struct process *current_process(void);
struct process *process_find(int pid);
void process_init(const uint32_t *relocs, uint32_t n);
int process_spawn(const char *path, int argc, char *const argv[]);
void process_exit(int status);
int process_wait(int pid, int *status);
void process_thread_gone(struct process *p);
void *process_sbrk(int increment);
int process_pid(void);
#define HEAP_DEFAULT    (32 * 1024)     /* a process's heap until it asks for more */

/* vfs.c */
struct vnode *vnode_new(const struct vnode_ops *ops, int type, void *fs,
                        uint32_t ino, uint32_t size);
void vnode_get(struct vnode *v);
void vnode_put(struct vnode *v);
int vfs_mount(const char *path, struct vnode *root);
int vfs_lookup(const char *path, struct vnode **out);
int vfs_open(const char *path, int flags);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, size_t len);
int vfs_write(int fd, const void *buf, size_t len);
int vfs_seek(int fd, int32_t off, int whence);
int vfs_readdir(int fd, struct dirent *de);
int vfs_stat(const char *path, struct stat *st);
int vfs_ioctl(int fd, int req, void *arg);

/* romfs.c, devfs.c */
struct vnode *romfs_init(const void *image);
struct vnode *devfs_init(void);
int dev_register(const char *name, const struct vnode_ops *ops, void *ctx);

/* boot.s */
void kernel_halt(int status);
int cpu_id(void);
long kernel_time(void);
extern struct thread *switch_from, *switch_to;

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
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int memcmp(const void *a, const void *b, size_t n);
char *strcpy(char *d, const char *s);
char *strcat(char *d, const char *s);
char *strchr(const char *s, int c);
void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);
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
void console_putc(int c);
void console_puts(const char *s);
struct thread *console_poll(void);
int console_getc(void);
int console_pending(void);
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
struct thread *thread_create_in(struct process *p, const char *name,
                                int (*fn)(void *), void *arg, int prio,
                                size_t stack_size);
void thread_kill_others(struct process *p);
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
void irq_dispatch(void);

/* whoever provides init_main: init.c, or a test */
void init_main(void);

#endif
