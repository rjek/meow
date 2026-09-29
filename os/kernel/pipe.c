/* Pipes: a ring with a reader's and a writer's wait queue.  Both ends
   share one vnode; a file's flags say which end it is, and the vnode
   counts the ends still open so that a reader sees end of file and a
   writer without a reader gets EPIPE. */
#include "kernel.h"

#define PIPE_SIZE 256

struct pipe {
    unsigned char buf[PIPE_SIZE];
    unsigned head, count;
    int readers, writers;
    struct waitq rq, wq;
};

static int pipe_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    struct pipe *p = v->fs;
    unsigned char *out = buf;
    size_t n = 0;

    (void)off;
    kenter();
    while (p->count == 0 && p->writers > 0) {
        waitq_wait(&p->rq);
    }
    while (n < len && p->count > 0) {
        out[n++] = p->buf[p->head];
        p->head = (p->head + 1) % PIPE_SIZE;
        p->count--;
    }
    if (n > 0) {
        preempt_if(waitq_wake_one(&p->wq));
    }
    kexit();
    return (int)n;
}

static int pipe_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    struct pipe *p = v->fs;
    const unsigned char *in = buf;
    size_t n = 0;

    (void)off;
    kenter();
    while (n < len) {
        if (p->readers == 0) {
            kexit();
            return n > 0 ? (int)n : -EPIPE;
        }
        if (p->count == PIPE_SIZE) {
            preempt_if(waitq_wake_one(&p->rq));
            waitq_wait(&p->wq);
            continue;
        }
        p->buf[(p->head + p->count) % PIPE_SIZE] = in[n++];
        p->count++;
    }
    preempt_if(waitq_wake_one(&p->rq));
    kexit();
    return (int)n;
}

static void pipe_release(struct vnode *v)
{
    kfree(v->fs);
}

static const struct vnode_ops pipe_ops = {
    NULL, pipe_read, pipe_write, NULL, NULL, NULL, NULL, pipe_release, NULL
};

/* An end has closed: whoever waits on the other end must know. */
void pipe_end_closed(struct vnode *v, int flags)
{
    struct pipe *p = v->fs;

    if ((flags & O_WRONLY) != 0) {
        p->writers--;
        while (waitq_wake_one(&p->rq) != NULL) {
        }
    } else {
        p->readers--;
        while (waitq_wake_one(&p->wq) != NULL) {
        }
    }
}

/* fds[0] to read from, fds[1] to write to */
int vfs_pipe(int fds[2])
{
    struct pipe *p;
    struct vnode *v;
    int r, w;

    kenter();
    p = kmalloc(sizeof *p);
    if (p == NULL) {
        kexit();
        return -ENOMEM;
    }
    memset(p, 0, sizeof *p);
    p->readers = 1;
    p->writers = 1;
    waitq_init(&p->rq);
    waitq_init(&p->wq);
    v = vnode_new(&pipe_ops, V_PIPE, p, 0, 0);
    if (v == NULL) {
        kfree(p);
        kexit();
        return -ENOMEM;
    }
    r = vfs_open_vnode(v, O_RDONLY);
    if (r < 0) {
        vnode_put(v);
        kexit();
        return r;
    }
    vnode_get(v);                       /* the second file's reference */
    w = vfs_open_vnode(v, O_WRONLY);
    if (w < 0) {
        vnode_put(v);
        vfs_close(r);
        kexit();
        return w;
    }
    fds[0] = r;
    fds[1] = w;
    kexit();
    return 0;
}
