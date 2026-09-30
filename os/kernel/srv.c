/* Services: a program makes a port, names device nodes and mount points
   that the port serves, and then answers requests.  Every operation on a
   vnode the port serves becomes a request: the calling thread queues a
   record of what it wants and blocks; the server takes the record with
   srv_recv(), does the work and wakes the caller with srv_reply().

   There is one address space, so nothing is copied.  The record lives
   on the caller's stack and the server is handed a pointer to it; the
   buffer in it is the caller's own, which the server reads or writes
   where it lies.  Both are safe for exactly as long as the caller is
   blocked, so everything here is about not letting the caller go while
   a server may still use them:

   - A server may keep a request as long as it likes, which is how a
     read waits for data.  If the caller is to be killed meanwhile, the
     request is delivered again with `cancelled` set, and the caller
     ends only when the server has answered it: it is marked doomed,
     the call it was making fails with EINTR, and it ends as it leaves
     the kernel, having let go of whatever the call held.
   - A port may have a time limit.  A server that stays away from
     srv_recv(), and from polling the port, for longer than that, while
     a request waits that it has not taken or has taken and not come
     back since, is taken for dead:
     its port is closed, its clients are given ETIMEDOUT, and it is
     killed, so that it never touches their memory again.  A request it
     took before it last came back is one it is keeping, and has no
     limit.
   - When a port closes, by the server's choice or its end, whatever was
     waiting is given EIO and the nodes and mounts go.  The port's
     memory stays until the last vnode that names it has gone too. */
#include "kernel.h"

enum { R_QUEUED, R_ACTIVE, R_DONE };

struct port;

struct request {
    struct srv_req r;                   /* what the server sees: keep first */
    struct request *next;               /* in the port's queue, to be received */
    struct request *anext;              /* among those the server holds */
    struct port *port;
    struct vnode *held;                 /* the vnode, kept until the answer */
    struct waitq wq;                    /* where the caller waits */
    int state;
    int queued;
    int result;
    uint32_t taken;                     /* the port's count of srv_recv() calls when it was */
};

/* what a server has said about a node's readiness; none means ready */
struct ready {
    struct ready *next;
    uint32_t node;
    int mask;
};

struct port {
    struct process *owner;
    struct request *head, *tail;
    struct request *active;
    struct waitq server;
    struct ready *ready;
    uint32_t timeout;                   /* ticks, or 0 for no limit */
    uint32_t last_recv;                 /* when the server last came for a request, or polled */
    uint32_t recvs;                     /* how often it has */
    int in_recv;                        /* it is waiting for one now */
    int flags;
    int dead;
    int refs;                           /* the port's own vnode, and each it serves */
};

static const struct vnode_ops port_ops, srv_ops;

/* ---- the queue ---- */

static void post(struct port *pt, struct request *r)
{
    r->next = NULL;
    r->queued = 1;
    if (pt->tail == NULL) {
        pt->head = r;
    } else {
        pt->tail->next = r;
    }
    pt->tail = r;
}

static void unpost(struct port *pt, struct request *r)
{
    struct request **pp, *q;

    for (pp = &pt->head; *pp != NULL; pp = &(*pp)->next) {
        if (*pp == r) {
            *pp = r->next;
            break;
        }
    }
    pt->tail = NULL;
    for (q = pt->head; q != NULL; q = q->next) {
        pt->tail = q;
    }
    r->queued = 0;
}

static struct thread *finish(struct request *r, int result)
{
    r->result = result;
    r->state = R_DONE;
    return waitq_wake_one(&r->wq);
}

static void port_put(struct port *pt)
{
    if (--pt->refs == 0) {
        while (pt->ready != NULL) {
            struct ready *y = pt->ready;

            pt->ready = y->next;
            kfree(y);
        }
        kfree(pt);
    }
}

/* The port closes: nothing more is asked of it, and everything that was
   being asked gets err. */
static void port_die(struct port *pt, int err)
{
    struct request *r;

    if (pt->dead != 0) {
        return;
    }
    pt->dead = 1;
    dev_unregister_owner(pt);
    vfs_umount_owner(pt);
    while ((r = pt->active) != NULL) {
        pt->active = r->anext;
        if (r->queued != 0) {
            unpost(pt, r);
        }
        finish(r, err);
    }
    while ((r = pt->head) != NULL) {
        unpost(pt, r);
        finish(r, err);
    }
    while (waitq_wake_one(&pt->server) != NULL) {
    }
    poll_wake();
}

/* ---- the caller's side ---- */

/* Ask pt to do op and wait for its answer.  v, if there is one, is the
   vnode concerned, kept alive until then. */
static int call(struct port *pt, struct vnode *v, struct request *r, int op,
                uint32_t node, void *buf, uint32_t len, uint32_t off)
{
    struct thread *t = this_cpu()->current;
    struct request *outer = t->req;
    int rc;

    kenter();
    if (pt->dead != 0) {
        kexit();
        return -EIO;
    }
    if (t->proc == pt->owner) {
        kexit();
        return -EDEADLK;                /* it would wait for itself */
    }
    r->r.op = op;                       /* field by field: memset is a byte at a time */
    r->r.node = node;
    r->r.buf = buf;
    r->r.len = len;
    r->r.off = off;
    r->r.pid = t->proc->pid;
    r->r.cancelled = 0;
    r->r.new_node = 0;
    r->r.type = v != NULL ? v->type : 0;
    r->r.size = v != NULL ? v->size : 0;
    r->port = pt;
    r->held = v;
    r->anext = NULL;
    r->state = R_QUEUED;
    r->result = 0;
    waitq_init(&r->wq);
    if (v != NULL) {
        vnode_get(v);
    }
    post(pt, r);
    t->req = r;
    waitq_wake_one(&pt->server);
    poll_wake();
    while (r->state != R_DONE) {
        if (pt->timeout == 0) {
            waitq_wait(&r->wq);
        } else if (waitq_wait_for(&r->wq, pt->timeout) == 0 && r->state != R_DONE &&
                   (r->state == R_QUEUED || r->taken == pt->recvs) &&
                   pt->in_recv == 0 && ticks_now() - pt->last_recv >= pt->timeout) {
            int pid = pt->owner->pid;

            port_die(pt, -ETIMEDOUT);
            process_kill(pid);
        }
    }
    t->req = outer;
    rc = r->result;
    if (r->held != NULL) {
        r->held = NULL;
        vnode_put(v);
    }
    if (t->doomed != 0) {
        rc = -EINTR;                    /* it goes when this call is over: see kexit() */
    }
    kexit();
    return rc;
}

/* Whether a server has t's request in hand */
int srv_holds(struct thread *t)
{
    return t->req != NULL && t->req->state == R_ACTIVE;
}

/* t is to be killed while it waits for an answer.  If the server has not
   seen the request it is withdrawn; if the answer has come, there is
   nothing to do; if the server holds it, it is delivered again marked
   cancelled, and t must wait.  Returns whether t must. */
int srv_abandon(struct thread *t)
{
    struct request *r = t->req;
    struct port *pt = r->port;

    if (r->state == R_ACTIVE) {
        if (r->r.cancelled == 0) {
            r->r.cancelled = 1;
            post(pt, r);
            preempt_if(waitq_wake_one(&pt->server));
            preempt_if(poll_wake());
        }
        return 1;
    }
    if (r->state == R_QUEUED) {
        unpost(pt, r);
    }
    t->req = NULL;
    if (r->held != NULL) {
        struct vnode *v = r->held;

        r->held = NULL;
        vnode_put(v);
    }
    return 0;
}

/* ---- the vnodes a port serves: fs is the port, ino the server's
   number for the file ---- */

struct vnode *srv_vnode(void *port, uint32_t node, int type, uint32_t size)
{
    struct vnode *v = vnode_new(&srv_ops, type, port, node, size);

    if (v != NULL) {
        ((struct port *)port)->refs++;
    }
    return v;
}

static int s_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    struct request r;
    int rc = call(dir->fs, dir, &r, SRV_LOOKUP, dir->ino, (void *)name,
                  (uint32_t)strlen(name), 0);

    if (rc < 0) {
        return rc;
    }
    *out = srv_vnode(dir->fs, r.r.new_node, r.r.type, r.r.size);
    return *out == NULL ? -ENOMEM : 0;
}

static int s_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    struct request r;

    return call(v->fs, v, &r, SRV_READ, v->ino, buf, (uint32_t)len, off);
}

static int s_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    struct request r;
    int rc = call(v->fs, v, &r, SRV_WRITE, v->ino, (void *)buf, (uint32_t)len, off);

    if (rc >= 0) {
        v->size = r.r.size;
    }
    return rc;
}

static int s_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    struct request r;

    return call(v->fs, v, &r, SRV_READDIR, v->ino, de, sizeof *de, index);
}

static int s_create(struct vnode *dir, const char *name, int type, struct vnode **out)
{
    struct request r;
    int rc = call(dir->fs, dir, &r, SRV_CREATE, dir->ino, (void *)name,
                  (uint32_t)strlen(name), (uint32_t)type);

    if (rc < 0) {
        return rc;
    }
    *out = srv_vnode(dir->fs, r.r.new_node, type, 0);
    return *out == NULL ? -ENOMEM : 0;
}

static int s_unlink(struct vnode *dir, const char *name)
{
    struct request r;

    return call(dir->fs, dir, &r, SRV_UNLINK, dir->ino, (void *)name,
                (uint32_t)strlen(name), 0);
}

static int s_ioctl(struct vnode *v, int req, void *arg)
{
    struct request r;

    return call(v->fs, v, &r, SRV_IOCTL, v->ino, arg, 0, (uint32_t)req);
}

static int s_truncate(struct vnode *v)
{
    struct request r;
    int rc = call(v->fs, v, &r, SRV_TRUNCATE, v->ino, NULL, 0, 0);

    if (rc >= 0) {
        v->size = r.r.size;
    }
    return rc;
}

static void s_release(struct vnode *v)
{
    struct port *pt = v->fs;
    struct request r;

    if ((pt->flags & SRV_WANT_RELEASE) != 0) {
        call(pt, NULL, &r, SRV_RELEASE, v->ino, NULL, 0, 0);
    }
    port_put(pt);
}

static int s_poll(struct vnode *v)
{
    struct port *pt = v->fs;
    struct ready *y;

    if (pt->dead != 0) {
        return POLLIN | POLLOUT | POLLHUP;
    }
    for (y = pt->ready; y != NULL; y = y->next) {
        if (y->node == v->ino) {
            return y->mask;
        }
    }
    return POLLIN | POLLOUT;
}

static const struct vnode_ops srv_ops = {
    s_lookup, s_read, s_write, s_readdir, s_create, s_unlink, s_ioctl,
    s_release, s_truncate, s_poll
};

/* ---- the port itself: a descriptor, so that it closes with whoever
   made it, and can be polled along with anything else ---- */

static void port_release(struct vnode *v)
{
    port_die(v->fs, -EIO);
    port_put(v->fs);
}

/* Readable when a request waits.  A server that polls its port has come
   back to it as surely as one that calls srv_recv(). */
static int port_poll(struct vnode *v)
{
    struct port *pt = v->fs;

    if (current_process() == pt->owner) {
        pt->last_recv = ticks_now();
        pt->recvs++;
    }
    return (pt->head != NULL ? POLLIN : 0) | (pt->dead != 0 ? POLLHUP : 0);
}

static const struct vnode_ops port_ops = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, port_release, NULL, port_poll
};

static struct port *port_of(int fd)
{
    struct vnode *v = vfs_fd_vnode(fd);

    return v != NULL && v->ops == &port_ops ? v->fs : NULL;
}

/* A port, as a descriptor.  timeout is how many ticks the server may
   stay away from srv_recv() while requests wait before it is given up
   on, or 0 for as long as it likes. */
int srv_create(int timeout, int flags)
{
    struct port *pt;
    struct vnode *v;
    int fd;

    if (timeout < 0) {
        return -EINVAL;
    }
    kenter();
    pt = kmalloc(sizeof *pt);
    if (pt == NULL) {
        kexit();
        return -ENOMEM;
    }
    memset(pt, 0, sizeof *pt);
    pt->owner = current_process();
    pt->timeout = (uint32_t)timeout;
    pt->flags = flags;
    pt->last_recv = ticks_now();
    pt->refs = 1;
    v = vnode_new(&port_ops, V_PORT, pt, 0, 0);
    if (v == NULL) {
        kfree(pt);
        kexit();
        return -ENOMEM;
    }
    fd = vfs_open_vnode(v, O_RDWR);
    if (fd < 0) {
        vnode_put(v);
    }
    kexit();
    return fd;
}

/* /dev/NAME, served by the port as its node */
int srv_dev(int fd, const char *name, uint32_t node)
{
    struct port *pt = port_of(fd);
    int rc;

    if (pt == NULL) {
        return -EBADF;
    }
    kenter();
    rc = dev_register_served(name, pt, node);
    kexit();
    return rc;
}

/* A file system at path, whose root directory is the port's node */
int srv_mount(int fd, const char *path, uint32_t root)
{
    struct port *pt = port_of(fd);
    struct vnode *v;
    int rc;

    if (pt == NULL) {
        return -EBADF;
    }
    kenter();
    v = srv_vnode(pt, root, V_DIR, 0);
    if (v == NULL) {
        kexit();
        return -ENOMEM;
    }
    rc = vfs_mount_owned(path, v, "user", pt);
    if (rc < 0) {
        pt->refs--;                     /* not through release: nothing to tell the server */
        kfree(v);
    }
    kexit();
    return rc;
}

/* The next request, waiting up to ticks for it: 0 does not wait, a
   negative number waits for ever.  Returns 1 with *req, or 0. */
int srv_recv(int fd, struct srv_req **req, int ticks)
{
    struct port *pt = port_of(fd);
    struct request *r;

    if (pt == NULL) {
        return -EBADF;
    }
    kenter();
    pt->last_recv = ticks_now();
    pt->recvs++;
    while (pt->head == NULL) {
        int woken = 1;

        if (pt->dead != 0 || ticks == 0) {
            kexit();
            return pt->dead != 0 ? -EIO : 0;
        }
        pt->in_recv++;
        if (ticks < 0) {
            waitq_wait(&pt->server);
        } else {
            woken = waitq_wait_for(&pt->server, (uint32_t)ticks);
        }
        pt->in_recv--;
        pt->last_recv = ticks_now();
        if (woken == 0) {
            ticks = 0;
        }
    }
    r = pt->head;
    unpost(pt, r);
    if (r->state == R_QUEUED) {
        r->state = R_ACTIVE;
        r->taken = pt->recvs;
        r->anext = pt->active;
        pt->active = r;
    }
    *req = &r->r;
    kexit();
    return 1;
}

/* Answer a request that srv_recv() gave out and wake whoever made it. */
int srv_reply(int fd, struct srv_req *req, int result)
{
    struct port *pt = port_of(fd);
    struct request **pp, *r;

    if (pt == NULL) {
        return -EBADF;
    }
    kenter();
    for (pp = &pt->active; (r = *pp) != NULL; pp = &r->anext) {
        if (&r->r == req) {
            break;
        }
    }
    if (r == NULL) {
        kexit();
        return -EINVAL;                 /* not one it holds: answered already, or gone */
    }
    *pp = r->anext;
    if (r->queued != 0) {
        unpost(pt, r);                  /* a cancellation it had not got to */
    }
    preempt_if(finish(r, result));
    kexit();
    return 0;
}

/* What poll should say of one of the port's nodes from now on.  Until a
   server says otherwise a node is always readable and writable. */
int srv_ready(int fd, uint32_t node, int mask)
{
    struct port *pt = port_of(fd);
    struct ready *y;

    if (pt == NULL) {
        return -EBADF;
    }
    kenter();
    for (y = pt->ready; y != NULL; y = y->next) {
        if (y->node == node) {
            break;
        }
    }
    if (y == NULL) {
        y = kmalloc(sizeof *y);
        if (y == NULL) {
            kexit();
            return -ENOMEM;
        }
        y->node = node;
        y->next = pt->ready;
        pt->ready = y;
    }
    y->mask = mask;
    preempt_if(poll_wake());
    kexit();
    return 0;
}
