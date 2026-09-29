/* devfs: the drivers as a directory.  A driver registers a name and
   its ops; /dev/NAME is a vnode with them. */
#include "kernel.h"

#define NDEV 16

struct dev {
    const char *name;
    const struct vnode_ops *ops;
    void *ctx;
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
    ndevs++;
    return 0;
}

static int devfs_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    unsigned i;

    (void)dir;
    for (i = 0; i < ndevs; i++) {
        if (strcmp(devs[i].name, name) == 0) {
            *out = vnode_new(devs[i].ops, V_DEV, devs[i].ctx, i + 1, 0);
            return *out == NULL ? -ENOMEM : 0;
        }
    }
    return -ENOENT;
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
    const char *p = buf;
    size_t i;

    (void)v;
    (void)off;
    for (i = 0; i < len; i++) {
        console_putc(p[i]);
    }
    return (int)len;
}

static const struct vnode_ops console_ops = {
    NULL, console_read, console_write, NULL, NULL, NULL, NULL, NULL, NULL
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
