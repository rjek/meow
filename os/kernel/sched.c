/* Threads.  Every switch happens in the interrupt bank: kernel code that
   wants one sets switch_wanted and makes the timer fire at once, and the
   tick handler does the rest through switch_from and switch_to, which
   boot.s acts on.  The kernel itself is never preempted: a tick that
   finds in_kernel set is only counted, and its work is done when the
   kernel call returns. */
#include "kernel.h"

struct cpu cpu0;
struct thread *switch_from, *switch_to;

static struct thread idle_thread;
static struct thread *ready_head[NPRIO], *ready_tail[NPRIO];
static struct thread *sleepers;         /* by wake time */
static struct thread *zombies;
static uint32_t ticks;

static void enqueue(struct thread *t)
{
    t->next = NULL;
    t->state = T_READY;
    if (ready_tail[t->prio] == NULL) {
        ready_head[t->prio] = t;
    } else {
        ready_tail[t->prio]->next = t;
    }
    ready_tail[t->prio] = t;
}

static struct thread *dequeue_best(void)
{
    int p;

    for (p = NPRIO - 1; p >= 0; p--) {
        struct thread *t = ready_head[p];

        if (t != NULL) {
            ready_head[p] = t->next;
            if (ready_head[p] == NULL) {
                ready_tail[p] = NULL;
            }
            return t;
        }
    }
    kpanic("nothing to run");
    return NULL;
}

static int best_ready_prio(void)
{
    int p;

    for (p = NPRIO - 1; p >= 0; p--) {
        if (ready_head[p] != NULL) {
            return p;
        }
    }
    return -1;
}

void sched_init(void)
{
    struct cpu *c = this_cpu();

    idle_thread.name = "idle";
    idle_thread.prio = PRIO_IDLE;
    idle_thread.state = T_RUNNING;
    c->current = &idle_thread;
    c->slice = SLICE;
}

/* Start the tick.  Called once the boot thread is ready to become idle. */
void sched_start(void)
{
    CH_MASK(0) = 1u << IRQ_TIMER;
    CH_TIMER_RELOAD = CH_TIMER_HZ / HZ;
}

struct thread *thread_current(void)
{
    return this_cpu()->current;
}

uint32_t ticks_now(void)
{
    return ticks;
}

void kenter(void)
{
    this_cpu()->in_kernel++;
}

static void tick_work(void);

void kexit(void)
{
    struct cpu *c = this_cpu();

    if (--c->in_kernel == 0 && c->tick_pending != 0) {
        c->in_kernel++;
        tick_work();
        c->in_kernel--;
    }
}

/* Hand the CPU over: the caller has already put itself where it should
   be (a queue, the sleepers, the zombies) or is still running and wants
   an equal to have a turn. */
void schedule(void)
{
    struct cpu *c = this_cpu();
    struct thread *t = c->current;

    if (t->state == T_RUNNING) {
        enqueue(t);
    }
    c->switch_wanted = 1;
    CH_TIMER_VALUE = 1;                 /* the tick fires after this store */
    while (c->switch_wanted != 0) {
    }
}

void thread_ready(struct thread *t)
{
    enqueue(t);
}

void thread_block(void)
{
    this_cpu()->current->state = T_BLOCKED;
    schedule();
}

void thread_yield(void)
{
    kenter();
    schedule();
    kexit();
}

void thread_sleep(uint32_t n)
{
    struct thread *t = this_cpu()->current;
    struct thread **pp;

    kenter();
    t->wake = ticks + n;
    t->state = T_SLEEPING;
    for (pp = &sleepers; *pp != NULL && (*pp)->wake <= t->wake; pp = &(*pp)->next) {
    }
    t->next = *pp;
    *pp = t;
    schedule();
    kexit();
}

struct thread *thread_create(const char *name, int (*fn)(void *), void *arg,
                             int prio, size_t stack_size)
{
    struct thread *t;

    kenter();
    t = kmalloc(sizeof *t);
    if (t == NULL) {
        kexit();
        return NULL;
    }
    memset(t, 0, sizeof *t);
    t->stack = kmalloc(stack_size);
    if (t->stack == NULL) {
        kfree(t);
        kexit();
        return NULL;
    }
    t->stack_size = stack_size;
    t->name = name;
    t->prio = prio;
    t->regs[0] = (uint32_t)arg;
    t->regs[R_SP] = ((uint32_t)t->stack + stack_size) & ~7u;
    t->regs[R_LR] = (uint32_t)thread_exit;   /* fn's return value is its status */
    t->regs[R_PC] = (uint32_t)fn;
    enqueue(t);
    if (prio > this_cpu()->current->prio) {
        schedule();
    }
    kexit();
    return t;
}

void thread_exit(int status)
{
    struct thread *t = this_cpu()->current;

    kenter();
    if (t == &idle_thread) {
        kpanic("idle exited");
    }
    t->exit_status = status;
    t->state = T_ZOMBIE;
    t->next = zombies;
    zombies = t;
    schedule();
    kpanic("zombie ran");
}

/* The idle thread's chores: free what has finished. */
void idle_work(void)
{
    kenter();
    while (zombies != NULL) {
        struct thread *t = zombies;

        zombies = t->next;
        kfree(t->stack);
        kfree(t);
    }
    kexit();
}

/* Choose the next thread and tell boot.s.  Runs in the interrupt bank. */
static void do_switch(void)
{
    struct cpu *c = this_cpu();
    struct thread *next = dequeue_best();

    c->switch_wanted = 0;
    c->slice = SLICE;
    if (next == c->current) {
        next->state = T_RUNNING;
        return;
    }
    switch_from = c->current;
    switch_to = next;
    c->current->in_kernel = c->in_kernel;
    c->in_kernel = next->in_kernel;
    c->current = next;
    next->state = T_RUNNING;
}

/* The tick's work, done with the kernel's data in a consistent state:
   in the handler when user code was interrupted, else when the kernel
   call that was interrupted returns. */
static void tick_work(void)
{
    struct cpu *c = this_cpu();
    struct thread *t = c->current;
    int woke = 0;

    ticks += (uint32_t)c->tick_pending;
    c->slice -= c->tick_pending;
    c->tick_pending = 0;
    while (sleepers != NULL && sleepers->wake <= ticks) {
        struct thread *s = sleepers;

        sleepers = s->next;
        enqueue(s);
        woke = 1;
    }
    if (t->state != T_RUNNING) {
        return;                         /* already going somewhere */
    }
    if ((woke != 0 && best_ready_prio() > t->prio) ||
        (c->slice <= 0 && best_ready_prio() >= t->prio)) {
        enqueue(t);
        c->switch_wanted = 1;
    }
    c->slice = c->slice <= 0 ? SLICE : c->slice;
}

void irq_dispatch(void)
{
    struct cpu *c = this_cpu();
    uint32_t pending = CH_PENDING & CH_MASK(0);

    switch_from = NULL;
    switch_to = NULL;
    if ((pending & (1u << IRQ_TIMER)) != 0) {
        CH_PENDING = 1u << IRQ_TIMER;
        if (c->switch_wanted != 0) {
            do_switch();                /* asked for by kernel code */
        } else {
            c->tick_pending++;
            if (c->in_kernel == 0) {
                tick_work();
                if (c->switch_wanted != 0) {
                    do_switch();
                }
            }
        }
    }
    if ((pending & ~(1u << IRQ_TIMER)) != 0) {
        CH_PENDING = pending & ~(1u << IRQ_TIMER);   /* nothing else yet */
    }
}
