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
static struct thread *all_threads;
static int next_tid = 1;
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
    idle_thread.proc = &kproc;
    all_threads = &idle_thread;
    kproc.nthreads = 1;
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

    if (c->in_kernel == 1 && c->current->doomed != 0) {
        /* told to end while a server held a request of its: now the
           server has let go, and so has the call that was waiting */
        struct process *p = c->current->proc;

        c->current->doomed = 0;
        if (p->dead == 0) {
            process_exit(p->killed);
        }
        thread_exit(0);
    }
    if (--c->in_kernel == 0 && c->tick_pending != 0) {
        c->in_kernel++;
        tick_work();
        c->in_kernel--;
        if (c->switch_wanted != 0) {    /* it queued us behind someone: go now */
            CH_TIMER_VALUE = 1;
            while (c->switch_wanted != 0) {
            }
        }
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
    return thread_create_in(current_process(), name, fn, arg, prio, stack_size);
}

struct thread *thread_create_in(struct process *p, const char *name,
                                int (*fn)(void *), void *arg, int prio,
                                size_t stack_size)
{
    struct thread *t;

    kenter();
    reap_zombies();
    reap_orphans();
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
    *(uint32_t *)t->stack = STACK_MAGIC;
    t->name = name;
    t->prio = prio;
    t->regs[0] = (uint32_t)arg;
    t->regs[R_SP] = ((uint32_t)t->stack + stack_size) & ~7u;
    t->regs[R_LR] = (uint32_t)thread_exit;   /* fn's return value is its status */
    t->regs[R_PC] = (uint32_t)fn;
    t->proc = p;
    t->tid = next_tid++;
    t->all = all_threads;
    all_threads = t;
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
    process_thread_gone(t->proc);
    schedule();
    kpanic("zombie ran");
}

static void unlink_from(struct thread **list, struct thread *t)
{
    for (; *list != NULL; list = &(*list)->next) {
        if (*list == t) {
            *list = t->next;
            return;
        }
    }
}

/* Take a thread out of wherever it is queued, for its end or to be
   sent elsewhere.  Never the running one. */
static void thread_kill(struct thread *t)
{
    struct thread *q;

    switch (t->state) {
    case T_READY:
        unlink_from(&ready_head[t->prio], t);
        ready_tail[t->prio] = NULL;     /* the tail may have been it */
        for (q = ready_head[t->prio]; q != NULL; q = q->next) {
            ready_tail[t->prio] = q;
        }
        break;
    case T_SLEEPING:
        unlink_from(&sleepers, t);
        break;
    case T_BLOCKED:
        if (t->waiting != NULL) {
            waitq_remove(t->waiting, t);
            t->waiting = NULL;
        }
        wait_forget(t);
        break;
    default:
        return;
    }
}

/* The end of a thread that is not the running one.  If a server holds a
   request of its, the server's pointers into its memory must go first:
   the server is told, and the thread ends when the answer wakes it.
   Returns whether it has gone. */
static int thread_end(struct thread *t)
{
    if (t->req != NULL && srv_abandon(t) != 0) {
        t->doomed = 1;
        return 0;
    }
    thread_kill(t);
    t->state = T_ZOMBIE;
    t->next = zombies;
    zombies = t;
    process_thread_gone(t->proc);
    return 1;
}

struct thread *thread_list(void)
{
    return all_threads;
}

int thread_id(void)
{
    return this_cpu()->current->tid;
}

/* Another thread in the calling program.  It runs fn(arg), and its
   return is its end.  The process's thread count goes up first, in case
   the new thread outranks the caller and has ended before this returns. */
int thread_spawn(int (*fn)(void *), void *arg, unsigned stack, int prio)
{
    struct process *p = current_process();
    struct thread *t;

    if (p == &kproc) {
        return -EINVAL;
    }
    if (prio < 1 || prio > PRIO_USER_MAX) {
        prio = PRIO_DEFAULT;
    }
    kenter();
    p->nthreads++;
    t = thread_create_in(p, p->name, fn, arg, prio, stack != 0 ? stack : STACK_USER);
    if (t == NULL) {
        p->nthreads--;
        kexit();
        return -ENOMEM;
    }
    kexit();
    return t->tid;
}

/* Every thread of p but the caller dies now, or as soon as the server
   it is waiting on lets it. */
void thread_kill_others(struct process *p)
{
    struct thread *t;

    for (t = all_threads; t != NULL; t = t->all) {
        if (t->proc == p && t != this_cpu()->current && t->state != T_ZOMBIE &&
            t->doomed == 0) {
            thread_end(t);
        }
    }
}

/* Another process is to end: p->killed says with what status.  One of
   its threads is sent to process_exit() in place of whatever it was
   doing, and the rest die here, so that none of its code runs again.  A
   thread that was inside the kernel is simply abandoned there, as one
   blocked there always has been at its process's exit.  If a server
   holds every one of them, the first to be answered does the exiting. */
void thread_kill_process(struct process *p)
{
    struct thread *t, *carrier = NULL;

    for (t = all_threads; t != NULL; t = t->all) {
        if (t->proc == p && t->state != T_ZOMBIE && srv_holds(t) == 0) {
            carrier = t;
            break;
        }
    }
    for (t = all_threads; t != NULL; t = t->all) {
        if (t->proc == p && t != carrier && t->state != T_ZOMBIE && t->doomed == 0) {
            thread_end(t);
        }
    }
    if (carrier == NULL) {
        return;
    }
    if (carrier->req != NULL) {
        srv_abandon(carrier);
    }
    thread_kill(carrier);
    carrier->regs[0] = (uint32_t)p->killed;
    carrier->regs[R_SP] = ((uint32_t)carrier->stack + carrier->stack_size) & ~7u;
    carrier->regs[R_LR] = (uint32_t)thread_exit;
    carrier->regs[R_PC] = (uint32_t)process_exit;
    carrier->in_kernel = 0;
    enqueue(carrier);
    if (carrier->prio > this_cpu()->current->prio) {
        schedule();
    }
}

/* Free the threads that have finished.  Any thread may do this from
   inside the kernel; a zombie is never running and never switched to. */
void reap_zombies(void)
{
    kenter();
    while (zombies != NULL) {
        struct thread *t = zombies, **pp;

        zombies = t->next;
        for (pp = &all_threads; *pp != t; pp = &(*pp)->all) {
        }
        *pp = t->all;
        kfree(t->stack);
        kfree(t);
    }
    kexit();
}

/* The idle thread's chores. */
void idle_work(void)
{
    reap_zombies();
    reap_orphans();
}

/* Choose the next thread and tell boot.s.  Runs in the interrupt bank. */
static void do_switch(void)
{
    struct cpu *c = this_cpu();
    struct thread *next = dequeue_best();

    c->switch_wanted = 0;
    c->slice = SLICE;
    if (c->current->stack != NULL && *(uint32_t *)c->current->stack != STACK_MAGIC) {
        kpanic("thread %s overran its %u byte stack", c->current->name,
               (unsigned)c->current->stack_size);
    }
    if (next == c->current) {
        next->state = T_RUNNING;
        return;
    }
    switch_from = c->current;
    switch_to = next;
    c->current->in_kernel = c->in_kernel;
    c->in_kernel = next->in_kernel;
    c->current = next;
    __client_sb = next->proc->sb;
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
    if (wait_expire(ticks) != 0) {
        woke = 1;
    }
    if (console_poll() != NULL) {
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
