/* devfs: the drivers as a directory.  A driver in the kernel registers
   a name and its ops, and /dev/NAME is a vnode with them; a program
   registers a name, its port and a number of its choosing, and
   /dev/NAME is a vnode whose every operation is a request to it. */
#include "kernel.h"

#define NDEV 16

struct dev {
    const char *name;
    const struct vnode_ops *ops;
    void *ctx;
    void *port;                         /* a served device's, or NULL */
    uint32_t node;
};

static struct dev devs[NDEV];
static unsigned ndevs;

int dev_register(const char *name, const struct vnode_ops *ops, void *ctx)
{
    if (ndevs == NDEV) {
        return -ENOSPC;
    }
    devs[ndevs].name = name;
    devs[ndevs].ops = ops;
    devs[ndevs].ctx = ctx;
    devs[ndevs].port = NULL;
    ndevs++;
    return 0;
}

static struct dev *find(const char *name)
{
    unsigned i;

    for (i = 0; i < ndevs; i++) {
        if (strcmp(devs[i].name, name) == 0) {
            return &devs[i];
        }
    }
    return NULL;
}

/* A device that a program serves.  The name is copied. */
int dev_register_served(const char *name, void *port, uint32_t node)
{
    char *copy;

    if (strlen(name) == 0 || strlen(name) > NAME_MAX || strchr(name, '/') != NULL) {
        return -EINVAL;
    }
    if (find(name) != NULL) {
        return -EEXIST;
    }
    if (ndevs == NDEV) {
        return -ENOSPC;
    }
    copy = kmalloc(strlen(name) + 1);
    if (copy == NULL) {
        return -ENOMEM;
    }
    strcpy(copy, name);
    devs[ndevs].name = copy;
    devs[ndevs].port = port;
    devs[ndevs].node = node;
    ndevs++;
    return 0;
}

/* The devices a port served go with it. */
void dev_unregister_owner(void *port)
{
    unsigned i, kept = 0;

    for (i = 0; i < ndevs; i++) {
        if (devs[i].port == port) {
            kfree((char *)devs[i].name);
        } else {
            devs[kept++] = devs[i];
        }
    }
    ndevs = kept;
}

static int devfs_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    struct dev *d = find(name);

    (void)dir;
    if (d == NULL) {
        return -ENOENT;
    }
    if (d->port != NULL) {
        *out = srv_vnode(d->port, d->node, V_DEV, 0);
    } else {
        *out = vnode_new(d->ops, V_DEV, d->ctx, (uint32_t)(d - devs) + 1, 0);
    }
    return *out == NULL ? -ENOMEM : 0;
}

static int devfs_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    (void)v;
    if (index >= ndevs) {
        return 0;
    }
    strcpy(de->name, devs[index].name);
    de->type = V_DEV;
    de->size = 0;
    return 1;
}

static const struct vnode_ops devfs_ops = {
    devfs_lookup, NULL, NULL, devfs_readdir, NULL, NULL, NULL, NULL, NULL
};

/* ---- the devices themselves ---- */

static int console_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    char *p = buf;
    size_t n = 0;

    (void)v;
    (void)off;
    /* block for the first byte, then take what is there up to a newline */
    while (n < len) {
        int c = console_getc();

        if (c < 0) {
            break;
        }
        p[n++] = (char)c;
        if (c == '\n' || console_pending() == 0) {
            break;
        }
    }
    return (int)n;
}

static int console_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    (void)v;
    (void)off;
    console_out(buf, len);
    return (int)len;
}

static int console_ready(struct vnode *v)
{
    (void)v;
    return POLLOUT | (console_readable() != 0 ? POLLIN : 0);
}

static const struct vnode_ops console_ops = {
    NULL, console_read, console_write, NULL, NULL, NULL, NULL, NULL, NULL, console_ready
};

static int null_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    (void)v; (void)buf; (void)len; (void)off;
    return 0;
}

static int null_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    (void)v; (void)buf; (void)off;
    return (int)len;
}

static const struct vnode_ops null_ops = {
    NULL, null_read, null_write, NULL, NULL, NULL, NULL, NULL, NULL
};

struct vnode *devfs_init(void)
{
    dev_register("console", &console_ops, NULL);
    dev_register("null", &null_ops, NULL);
    return vnode_new(&devfs_ops, V_DIR, NULL, 0, 0);
}
