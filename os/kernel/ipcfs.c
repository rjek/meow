/* ipcfs: named message queues and semaphores, mounted at /ipc, so that a
   program reaches them through ordinary descriptors that its children
   inherit.  A queue's write sends one message and its read receives one;
   a semaphore's read waits and its write posts.  Both block; ioctl has
   the versions that do not.  An object lives while it has a name or an
   open descriptor. */
#include "kernel.h"

struct ipcnode {
    char name[NAME_MAX + 1];
    int kind;
    struct ipcnode *next;
    int links;                          /* 1 while named, plus open vnodes */
    struct mq q;
    struct sem s;
};

static struct ipcnode *nodes;
static const struct vnode_ops ipc_ops, ipcdir_ops;

static uint32_t node_size(const struct ipcnode *n)
{
    return n->kind == IPC_MQ ? n->q.count : (uint32_t)(n->s.count > 0 ? n->s.count : 0);
}

static void node_put(struct ipcnode *n)
{
    if (--n->links == 0) {
        if (n->kind == IPC_MQ) {
            mq_destroy(&n->q);
        }
        kfree(n);
    }
}

static struct ipcnode *find(const char *name)
{
    struct ipcnode *n;

    for (n = nodes; n != NULL; n = n->next) {
        if (strcmp(n->name, name) == 0) {
            return n;
        }
    }
    return NULL;
}

/* Make a queue of b messages of a bytes, or a semaphore counting from a. */
int ipc_create(const char *name, int kind, int a, int b)
{
    struct ipcnode *n, **pp;

    if (strlen(name) == 0 || strlen(name) > NAME_MAX || strchr(name, '/') != NULL) {
        return -EINVAL;
    }
    if ((kind == IPC_MQ && (a <= 0 || b <= 0 || a > 4096 || b > 1024)) ||
        (kind == IPC_SEM && a < 0) || (kind != IPC_MQ && kind != IPC_SEM)) {
        return -EINVAL;
    }
    kenter();
    if (find(name) != NULL) {
        kexit();
        return -EEXIST;
    }
    n = kmalloc(sizeof *n);
    if (n == NULL) {
        kexit();
        return -ENOMEM;
    }
    memset(n, 0, sizeof *n);
    strcpy(n->name, name);
    n->kind = kind;
    n->links = 1;
    if (kind == IPC_MQ) {
        if (mq_init(&n->q, (size_t)a, (unsigned)b) < 0) {
            kfree(n);
            kexit();
            return -ENOMEM;
        }
    } else {
        sem_init(&n->s, a);
    }
    for (pp = &nodes; *pp != NULL; pp = &(*pp)->next) {
    }
    *pp = n;
    kexit();
    return 0;
}

static int ipcdir_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    struct ipcnode *n = find(name);

    (void)dir;
    if (n == NULL) {
        return -ENOENT;
    }
    *out = vnode_new(&ipc_ops, n->kind == IPC_MQ ? V_MQ : V_SEM, n, 0, node_size(n));
    if (*out == NULL) {
        return -ENOMEM;
    }
    n->links++;
    return 0;
}

static int ipcdir_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    struct ipcnode *n = nodes;

    (void)v;
    while (n != NULL && index > 0) {
        n = n->next;
        index--;
    }
    if (n == NULL) {
        return 0;
    }
    strcpy(de->name, n->name);
    de->type = n->kind == IPC_MQ ? V_MQ : V_SEM;
    de->size = node_size(n);
    return 1;
}

static int ipcdir_unlink(struct vnode *dir, const char *name)
{
    struct ipcnode **pp;

    (void)dir;
    for (pp = &nodes; *pp != NULL; pp = &(*pp)->next) {
        struct ipcnode *n = *pp;

        if (strcmp(n->name, name) == 0) {
            *pp = n->next;
            node_put(n);
            return 0;
        }
    }
    return -ENOENT;
}

static int ipc_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    struct ipcnode *n = v->fs;

    (void)off;
    if (n->kind == IPC_SEM) {
        sem_wait(&n->s);
        if (len > 0) {
            *(char *)buf = 1;
        }
        return 1;
    }
    if (len < n->q.msgsize) {
        return -EINVAL;
    }
    mq_receive(&n->q, buf, 1);
    return (int)n->q.msgsize;
}

static int ipc_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    struct ipcnode *n = v->fs;

    (void)off;
    if (n->kind == IPC_SEM) {
        sem_post(&n->s);
        return (int)len;
    }
    if (len != n->q.msgsize) {
        return -EINVAL;
    }
    mq_send(&n->q, buf, 1);
    return (int)len;
}

static int ipc_ioctl(struct vnode *v, int req, void *arg)
{
    struct ipcnode *n = v->fs;

    switch (req) {
    case IPC_TRYSEND:
        return n->kind == IPC_MQ ? mq_send(&n->q, arg, 0) : -ENOTTY;
    case IPC_TRYRECV:
        return n->kind == IPC_MQ ? mq_receive(&n->q, arg, 0) : -ENOTTY;
    case IPC_TRYWAIT:
        return n->kind == IPC_SEM ? sem_trywait(&n->s) : -ENOTTY;
    case IPC_VALUE:
        return n->kind == IPC_MQ ? (int)n->q.count : n->s.count;
    default:
        return -ENOTTY;
    }
}

static void ipc_release(struct vnode *v)
{
    node_put(v->fs);
}

static const struct vnode_ops ipcdir_ops = {
    ipcdir_lookup, NULL, NULL, ipcdir_readdir, NULL, ipcdir_unlink, NULL, NULL, NULL
};

static const struct vnode_ops ipc_ops = {
    NULL, ipc_read, ipc_write, NULL, NULL, NULL, ipc_ioctl, ipc_release, NULL
};

struct vnode *ipcfs_init(void)
{
    return vnode_new(&ipcdir_ops, V_DIR, NULL, 0, 0);
}
