/* Waiting and waking: wait queues, and on them semaphores, mutexes and
   message queues.  All of it runs inside the kernel, so nothing here
   needs a lock: a thread is only ever switched away from at a
   schedule(), and the tick's own work is deferred while in_kernel. */
#include "kernel.h"

void waitq_init(struct waitq *q)
{
    q->head = NULL;
    q->tail = NULL;
}

/* Block the current thread on q until someone wakes it. */
void waitq_wait(struct waitq *q)
{
    struct thread *t = this_cpu()->current;

    t->next = NULL;
    if (q->tail == NULL) {
        q->head = t;
    } else {
        q->tail->next = t;
    }
    q->tail = t;
    thread_block();
}

/* Make the longest waiter ready.  Returns it, or NULL.  The caller
   decides whether it should run now: thread code calls preempt_if(),
   the tick does its own check. */
struct thread *waitq_wake_one(struct waitq *q)
{
    struct thread *t = q->head;

    if (t == NULL) {
        return NULL;
    }
    q->head = t->next;
    if (q->head == NULL) {
        q->tail = NULL;
    }
    thread_ready(t);
    return t;
}

/* Give the CPU up now if t outranks the caller. */
void preempt_if(struct thread *t)
{
    if (t != NULL && t->prio > this_cpu()->current->prio) {
        schedule();
    }
}

/* ---- semaphores ---- */

void sem_init(struct sem *s, int count)
{
    s->count = count;
    waitq_init(&s->q);
}

void sem_wait(struct sem *s)
{
    kenter();
    while (s->count <= 0) {
        waitq_wait(&s->q);
    }
    s->count--;
    kexit();
}

int sem_trywait(struct sem *s)
{
    int got;

    kenter();
    got = s->count > 0;
    if (got != 0) {
        s->count--;
    }
    kexit();
    return got;
}

void sem_post(struct sem *s)
{
    kenter();
    s->count++;
    preempt_if(waitq_wake_one(&s->q));
    kexit();
}

/* ---- mutexes: not recursive, and the owner runs at the highest
   priority of anyone waiting for it until it lets go ---- */

void mutex_init(struct mutex *m)
{
    m->owner = NULL;
    waitq_init(&m->q);
}

void mutex_lock(struct mutex *m)
{
    struct thread *t = this_cpu()->current;

    kenter();
    while (m->owner != NULL) {
        if (m->owner == t) {
            kpanic("%s locks a mutex it holds", t->name);
        }
        if (m->owner->prio < t->prio) {
            m->owner->prio = t->prio;   /* inherit; restored on unlock */
        }
        waitq_wait(&m->q);
    }
    m->owner = t;
    m->owner_prio = t->prio;
    kexit();
}

void mutex_unlock(struct mutex *m)
{
    struct thread *t = this_cpu()->current;
    struct thread *w;

    kenter();
    if (m->owner != t) {
        kpanic("%s unlocks a mutex it does not hold", t->name);
    }
    t->prio = m->owner_prio;
    m->owner = NULL;
    w = waitq_wake_one(&m->q);
    preempt_if(w);
    kexit();
}

/* ---- message queues: fixed-size messages in a ring ---- */

int mq_init(struct mq *q, size_t msgsize, unsigned depth)
{
    q->buf = kmalloc(msgsize * depth);
    if (q->buf == NULL) {
        return -1;
    }
    q->msgsize = msgsize;
    q->depth = depth;
    q->head = 0;
    q->count = 0;
    waitq_init(&q->readers);
    waitq_init(&q->writers);
    return 0;
}

void mq_destroy(struct mq *q)
{
    kfree(q->buf);
    q->buf = NULL;
}

/* Returns 1 if sent, 0 if full and not blocking. */
int mq_send(struct mq *q, const void *msg, int block)
{
    kenter();
    while (q->count == q->depth) {
        if (block == 0) {
            kexit();
            return 0;
        }
        waitq_wait(&q->writers);
    }
    memcpy(q->buf + ((q->head + q->count) % q->depth) * q->msgsize, msg, q->msgsize);
    q->count++;
    preempt_if(waitq_wake_one(&q->readers));
    kexit();
    return 1;
}

/* Returns 1 if received, 0 if empty and not blocking. */
int mq_receive(struct mq *q, void *msg, int block)
{
    kenter();
    while (q->count == 0) {
        if (block == 0) {
            kexit();
            return 0;
        }
        waitq_wait(&q->readers);
    }
    memcpy(msg, q->buf + q->head * q->msgsize, q->msgsize);
    q->head = (q->head + 1) % q->depth;
    q->count--;
    preempt_if(waitq_wake_one(&q->writers));
    kexit();
    return 1;
}
