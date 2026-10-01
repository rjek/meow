/* Threads.  Every switch happens in the interrupt bank: kernel code that
   wants one sets switch_wanted and rings its own doorbell, and the
   handler does the rest through switch_from and switch_to, which boot.s
   acts on.  The kernel itself is never preempted: a tick that finds
   in_kernel set is only counted, and its work is done when the kernel
   call returns.

   Each CPU has a scheduler of its own: its run queues, its idle thread
   and its current thread, in its local memory, and a thread stays on
   the CPU it was made for.  What is shared, the lists of all, sleeping
   and finished threads and everything the rest of the kernel keeps, is
   consistent because only one CPU is ever in the kernel: Chairman lock
   0 is held by a CPU exactly while its in_kernel is above zero.  kenter
   takes it on the step from 0 to 1, kexit drops it on the step back,
   and the handler, which switches between threads with different
   counts, leaves it as the new count says. */
#include "kernel.h"

static struct thread *all_threads;
static int next_tid = 1;
static struct thread *sleepers;         /* by wake time */
static struct thread *zombies;
static uint32_t ticks;
static int ncpus_online = 1;

static void klock(void)
{
    while (CH_LOCK(0) != 0) {           /* the read is the taking */
    }
}

static void kunlock(void)
{
    CH_LOCK(0) = 0;
}

/* Put t on its CPU's queue; a CPU other than this one is rung so that
   it looks at what it has. */
static void enqueue(struct thread *t)
{
    struct cpu *c = cpu_of(t->cpu);

    t->next = NULL;
    t->state = T_READY;
    if (c->ready_tail[t->prio] == NULL) {
        c->ready_head[t->prio] = t;
    } else {
        c->ready_tail[t->prio]->next = t;
    }
    c->ready_tail[t->prio] = t;
    if (t->cpu != this_cpu()->cpu) {
        CH_DOORBELL(t->cpu) = 1;
    }
}

/* Take t off its CPU's queue, wherever it is in it. */
static void unqueue(struct thread *t)
{
    struct cpu *c = cpu_of(t->cpu);
    struct thread **pp, *q;

    for (pp = &c->ready_head[t->prio]; *pp != NULL; pp = &(*pp)->next) {
        if (*pp == t) {
            *pp = t->next;
            break;
        }
    }
    c->ready_tail[t->prio] = NULL;      /* the tail may have been it */
    for (q = c->ready_head[t->prio]; q != NULL; q = q->next) {
        c->ready_tail[t->prio] = q;
    }
}

static struct thread *dequeue_best(void)
{
    struct cpu *c = this_cpu();
    int p;

    for (p = NPRIO - 1; p >= 0; p--) {
        struct thread *t = c->ready_head[p];

        if (t != NULL) {
            c->ready_head[p] = t->next;
            if (c->ready_head[p] == NULL) {
                c->ready_tail[p] = NULL;
            }
            return t;
        }
    }
    kpanic("nothing to run");
    return NULL;
}

static int best_ready_prio(void)
{
    struct cpu *c = this_cpu();
    int p;

    for (p = NPRIO - 1; p >= 0; p--) {
        if (c->ready_head[p] != NULL) {
            return p;
        }
    }
    return -1;
}

/* The idle thread of CPU n, which is what runs when nothing else can,
   and the boot thread on CPU 0. */
static void idle_init(struct thread *t, int n)
{
    memset(t, 0, sizeof *t);
    t->name = "idle";
    t->prio = PRIO_IDLE;
    t->state = T_RUNNING;
    t->proc = &kproc;
    t->cpu = n;
    t->all = all_threads;
    all_threads = t;
    kproc.nthreads++;
}

void sched_init(void)
{
    struct cpu *c = this_cpu();
    static struct thread idle0;

    memset(c, 0, sizeof *c);
    idle_init(&idle0, 0);
    c->current = &idle0;
    c->idle = &idle0;
    c->slice = SLICE;
    c->online = 1;
}

/* Start the tick on this CPU, and listen for the doorbell.  Called once
   the boot thread is ready to become idle. */
void sched_start(void)
{
    CH_MASK(this_cpu()->cpu) = (1u << IRQ_TIMER) | (1u << IRQ_DOORBELL);
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

/* in_kernel goes up before the lock is taken and comes down after it is
   dropped, so that the handler, which sees the count, never takes the
   lock while this CPU holds it. */
void kenter(void)
{
    struct cpu *c = this_cpu();

    if (c->in_kernel++ == 0) {
        klock();
    }
}

static void tick_work(void);
static void force_switch(void);

void kexit(void)
{
    struct cpu *c = this_cpu();

    if (c->in_kernel == 1 && c->current->doomed != 0) {
        /* told to end: by a kill, or because a server has let go of a
           request of its and the call that was waiting has too */
        struct process *p = c->current->proc;

        c->current->doomed = 0;
        if (p->dead == 0) {
            process_exit(p->killed);
        }
        thread_exit(0);
    }
    if (c->in_kernel == 1 && (c->tick_pending != 0 || c->poked != 0)) {
        tick_work();
        if (c->switch_wanted != 0) {    /* it queued us behind someone: go now */
            force_switch();
        }
    }
    if (c->in_kernel == 1) {
        kunlock();
    }
    c->in_kernel--;
}

/* Ring our own doorbell and wait for the handler to switch us out. */
static void force_switch(void)
{
    struct cpu *c = this_cpu();

    CH_DOORBELL(c->cpu) = 1;
    while (c->switch_wanted != 0) {
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
    force_switch();
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
    return thread_create_in(current_process(), name, fn, arg, prio, stack_size,
                            this_cpu()->cpu);
}

struct thread *thread_create_on(const char *name, int (*fn)(void *), void *arg,
                                int prio, size_t stack_size, int cpu)
{
    return thread_create_in(current_process(), name, fn, arg, prio, stack_size, cpu);
}

/* A thread on CPU cpu, which may carry CPU_EXCLUSIVE: then it is the
   only thread the CPU runs, and the CPU's tick stops while it lives, so
   that nothing interrupts it but what it asks for. */
struct thread *thread_create_in(struct process *p, const char *name,
                                int (*fn)(void *), void *arg, int prio,
                                size_t stack_size, int cpu)
{
    struct thread *t;
    struct cpu *c;
    int exclusive = (cpu & CPU_EXCLUSIVE) != 0;

    cpu &= NCPU - 1;
    kenter();
    reap_zombies();
    reap_orphans();
    c = cpu_of(cpu);
    if (cpu >= ncpus_online || c->online == 0 || c->exclusive != NULL ||
        (exclusive != 0 && cpu == 0)) {
        kexit();
        return NULL;
    }
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
    t->cpu = cpu;
    t->regs[0] = (uint32_t)arg;
    t->regs[R_SP] = ((uint32_t)t->stack + stack_size) & ~7u;
    t->regs[R_LR] = (uint32_t)thread_exit;   /* fn's return value is its status */
    t->regs[R_PC] = (uint32_t)fn;
    t->proc = p;
    t->tid = next_tid++;
    t->all = all_threads;
    all_threads = t;
    if (exclusive != 0) {
        c->exclusive = t;
        CH_MASK(cpu) = 1u << IRQ_DOORBELL;
        CH_CPU_TIMER_RELOAD(cpu) = 0;
    }
    enqueue(t);
    if (cpu == this_cpu()->cpu && prio > this_cpu()->current->prio) {
        schedule();
    }
    kexit();
    return t;
}

/* t is finished with: the lists, and the CPU it had to itself */
static void thread_gone(struct thread *t)
{
    struct cpu *c = cpu_of(t->cpu);

    t->state = T_ZOMBIE;
    t->next = zombies;
    zombies = t;
    if (c->exclusive == t) {
        c->exclusive = NULL;
        CH_CPU_TIMER_RELOAD(t->cpu) = CH_TIMER_HZ / HZ;
        CH_MASK(t->cpu) = (1u << IRQ_TIMER) | (1u << IRQ_DOORBELL);
    }
    process_thread_gone(t->proc);
}

void thread_exit(int status)
{
    struct thread *t = this_cpu()->current;

    kenter();
    if (t == this_cpu()->idle) {
        kpanic("idle exited");
    }
    t->exit_status = status;
    thread_gone(t);
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
    switch (t->state) {
    case T_READY:
        unqueue(t);
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

/* A thread that is running on another CPU, as a thread other than the
   caller in the running state must be, cannot be ended from here: it is
   marked, its CPU is rung, and it ends itself at its next kernel exit
   or, if it is in user code, when its CPU next switches it out. */
static void doom(struct thread *t)
{
    t->doomed = 1;
    CH_DOORBELL(t->cpu) = 1;
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
    if (t->state == T_RUNNING) {
        doom(t);
        return 0;
    }
    thread_kill(t);
    thread_gone(t);
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
int thread_spawn_on(int (*fn)(void *), void *arg, unsigned stack, int prio, int cpu)
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
    t = thread_create_in(p, p->name, fn, arg, prio, stack != 0 ? stack : STACK_USER, cpu);
    if (t == NULL) {
        p->nthreads--;
        kexit();
        return -ENOMEM;
    }
    kexit();
    return t->tid;
}

int thread_spawn(int (*fn)(void *), void *arg, unsigned stack, int prio)
{
    return thread_spawn_on(fn, arg, stack, prio, this_cpu()->cpu);
}

/* Every thread of p but the caller dies now, or as soon as the server
   it is waiting on lets it, or as soon as its CPU hears. */
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
   holds every one of them, the first to be answered does the exiting.
   One running on another CPU does the exiting itself, when told. */
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
    if (carrier->state == T_RUNNING) {
        doom(carrier);
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
    if (carrier->cpu == this_cpu()->cpu && carrier->prio > this_cpu()->current->prio) {
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

/* What a doomed thread that was in user code runs when its CPU next
   picks it: a kernel call with nothing in it, whose exit does the
   ending. */
static int doomed_run(void *arg)
{
    (void)arg;
    kenter();
    kexit();
    return 0;
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
        if (next->doomed != 0 && c->in_kernel == 0) {
            /* its registers must be saved before they can be rewritten:
               idle has a turn */
            enqueue(next);
            next = c->idle;
            unqueue(next);
        } else {
            next->state = T_RUNNING;
            return;
        }
    }
    if (next->doomed != 0 && next->in_kernel == 0) {
        next->regs[R_SP] = ((uint32_t)next->stack + next->stack_size) & ~7u;
        next->regs[R_LR] = (uint32_t)thread_exit;
        next->regs[R_PC] = (uint32_t)doomed_run;
    }
    c->switch_from = c->current;
    c->switch_to = next;
    c->current->in_kernel = c->in_kernel;
    c->in_kernel = next->in_kernel;
    c->current = next;
    c->client_sb = next->proc->sb;
    next->state = T_RUNNING;
}

/* The tick's work, done with the kernel's data in a consistent state:
   in the handler when user code was interrupted, else when the kernel
   call that was interrupted returns.  The clock, the sleepers and the
   console are CPU 0's; every CPU keeps its own slices and looks at its
   queue when rung. */
static void tick_work(void)
{
    struct cpu *c = this_cpu();
    struct thread *t = c->current;
    int woke = c->poked;

    if (c->cpu == 0) {
        ticks += (uint32_t)c->tick_pending;
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
    }
    c->slice -= c->tick_pending;
    c->tick_pending = 0;
    c->poked = 0;
    if (t->state != T_RUNNING) {
        return;                         /* already going somewhere */
    }
    if (t->doomed != 0 && c->in_kernel == 1 && t != c->idle) {
        /* in user code when told to end: out, so that it can be sent
           to its end (in_kernel is 1 for the handler's own entry) */
        enqueue(t);
        c->switch_wanted = 1;
        return;
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
    uint32_t pending = CH_PENDING & CH_MASK(c->cpu);
    int held = c->in_kernel > 0;

    c->switch_from = NULL;
    c->switch_to = NULL;
    CH_PENDING = pending;
    if ((pending & (1u << IRQ_TIMER)) != 0) {
        c->tick_pending++;
    }
    if ((pending & (1u << IRQ_DOORBELL)) != 0) {
        c->poked = 1;
    }
    if (held != 0) {
        /* in a kernel call, which holds the lock, or is about to: only
           what it asked for */
        if (c->switch_wanted != 0) {
            do_switch();
        }
    } else {
        klock();
        c->in_kernel = 1;               /* for tick_work: the handler's own */
        tick_work();
        c->in_kernel = 0;
        if (c->switch_wanted != 0) {
            do_switch();
        }
    }
    if (c->in_kernel == 0) {
        kunlock();
    }
}

/* ---- the other CPUs ---- */

int cpu_count(void)
{
    return ncpus_online;
}

/* Bring CPU n up as an idle thread of its own, with its stacks and its
   state in its local memory, which it finds at LOCAL_BASE when it
   starts at cpu_entry. */
int cpu_start(int n)
{
    struct cpu *c = cpu_of(n);
    struct thread *idle;
    char *irq_stack, *stack;

    if (n <= 0 || n >= NCPU || (CH_PRESENT & (1u << n)) == 0 || c->online != 0) {
        return -EINVAL;
    }
    kenter();
    idle = kmalloc(sizeof *idle);
    irq_stack = kmalloc(IRQ_STACK);
    stack = kmalloc(2 * STACK_DEFAULT);
    if (idle == NULL || irq_stack == NULL || stack == NULL) {
        kfree(irq_stack);
        kfree(stack);
        kfree(idle);
        kexit();
        return -ENOMEM;
    }
    memset(c, 0, sizeof *c);
    c->cpu = n;
    idle_init(idle, n);
    idle->stack = stack;
    idle->stack_size = 2 * STACK_DEFAULT;
    *(uint32_t *)idle->stack = STACK_MAGIC;
    c->current = idle;
    c->idle = idle;
    c->slice = SLICE;
    c->boot_sp = ((uint32_t)idle->stack + 2 * STACK_DEFAULT) & ~7u;
    c->irq_sp = ((uint32_t)irq_stack + IRQ_STACK) & ~7u;
    CH_CPU_START(n) = (uint32_t)cpu_entry;
    CH_CPU_CONTROL(n) = 1;
    while (c->online == 0) {            /* it says so before anything else */
    }
    if (n >= ncpus_online) {
        ncpus_online = n + 1;
    }
    kexit();
    return 0;
}

/* A CPU other than 0 begins here, on its idle stack, and stays idle
   until something is put on its queue. */
void cpu_main(void)
{
    struct cpu *c = this_cpu();

    c->client_sb = 0;
    sched_start();
    c->online = 1;
    for (;;) {
        idle_work();
        cpu_wfi();
    }
}
