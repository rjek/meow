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

enum thread_state { T_READY, T_RUNNING, T_SLEEPING, T_BLOCKED, T_ZOMBIE };

struct thread {
    uint32_t regs[16];                  /* r0 to pc, saved by boot.s: keep first */
    struct thread *next;                /* the run, sleep or zombie list */
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

/* boot.s */
void kernel_halt(int status);
int cpu_id(void);
extern struct thread *switch_from, *switch_to;

/* lib.c */
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strcpy(char *d, const char *s);
void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);
void kpanic(const char *fmt, ...);

/* console.c */
void console_init(void);
void console_putc(int c);
void console_puts(const char *s);

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
