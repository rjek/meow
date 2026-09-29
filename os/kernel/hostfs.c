/* hostfs: a directory on the development host, through msim's BNV #-18,
   mounted at /host when there is one.  Each vnode knows its path in the
   host directory and opens a host file lazily on first read or write. */
#include "kernel.h"

enum { OP_PROBE, OP_OPEN, OP_CLOSE, OP_READ, OP_WRITE, OP_STAT, OP_READDIR,
       OP_MKDIR, OP_UNLINK };

struct hnode {
    int handle;                         /* the host's, or -1 */
    char path[PATH_MAX];                /* relative to the host directory */
};

static const struct vnode_ops hostfs_ops;

static struct vnode *hnode_vnode(const char *path, int type, uint32_t size)
{
    struct hnode *h = kmalloc(sizeof *h);
    struct vnode *v;

    if (h == NULL) {
        return NULL;
    }
    h->handle = -1;
    strcpy(h->path, path);
    v = vnode_new(&hostfs_ops, type, h, 0, size);
    if (v == NULL) {
        kfree(h);
    }
    return v;
}

static int join(char *out, const struct hnode *dir, const char *name)
{
    if (strlen(dir->path) + 1 + strlen(name) + 1 > PATH_MAX) {
        return -ENAMETOOLONG;
    }
    strcpy(out, dir->path);
    if (out[0] != '\0') {
        strcat(out, "/");
    }
    strcat(out, name);
    return 0;
}

static int hostfs_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    char path[PATH_MAX];
    struct stat st;
    int rc = join(path, dir->fs, name);

    if (rc == 0) {
        rc = host_call(OP_STAT, (int)path, (int)&st, 0, 0);
    }
    if (rc < 0) {
        return rc;
    }
    *out = hnode_vnode(path, st.type, st.size);
    return *out == NULL ? -ENOMEM : 0;
}

/* A handle, read-write if the host allows it */
static int hostfs_handle(struct hnode *h)
{
    if (h->handle < 0) {
        h->handle = host_call(OP_OPEN, (int)h->path, O_RDWR, 0, 0);
        if (h->handle < 0) {
            h->handle = host_call(OP_OPEN, (int)h->path, O_RDONLY, 0, 0);
        }
    }
    return h->handle;
}

static int hostfs_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    int fd = hostfs_handle(v->fs);

    return fd < 0 ? fd : host_call(OP_READ, fd, (int)buf, (int)len, (int)off);
}

static int hostfs_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    int fd = hostfs_handle(v->fs), n;

    if (fd < 0) {
        return fd;
    }
    n = host_call(OP_WRITE, fd, (int)buf, (int)len, (int)off);
    if (n > 0 && off + (uint32_t)n > v->size) {
        v->size = off + (uint32_t)n;
    }
    return n;
}

static int hostfs_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    return host_call(OP_READDIR, (int)((struct hnode *)v->fs)->path, (int)index, (int)de, 0);
}

static int hostfs_create(struct vnode *dir, const char *name, int type, struct vnode **out)
{
    char path[PATH_MAX];
    int rc = join(path, dir->fs, name);

    if (rc < 0) {
        return rc;
    }
    if (type == V_DIR) {
        rc = host_call(OP_MKDIR, (int)path, 0, 0, 0);
    } else {
        rc = host_call(OP_OPEN, (int)path, O_WRONLY | O_CREAT, 0, 0);
        if (rc >= 0) {
            host_call(OP_CLOSE, rc, 0, 0, 0);
            rc = 0;
        }
    }
    if (rc < 0) {
        return rc;
    }
    *out = hnode_vnode(path, type, 0);
    return *out == NULL ? -ENOMEM : 0;
}

static int hostfs_unlink(struct vnode *dir, const char *name)
{
    char path[PATH_MAX];
    int rc = join(path, dir->fs, name);

    return rc < 0 ? rc : host_call(OP_UNLINK, (int)path, 0, 0, 0);
}

static int hostfs_truncate(struct vnode *v)
{
    struct hnode *h = v->fs;
    int fd = host_call(OP_OPEN, (int)h->path, O_WRONLY | O_TRUNC, 0, 0);

    if (fd < 0) {
        return fd;
    }
    host_call(OP_CLOSE, fd, 0, 0, 0);
    v->size = 0;
    return 0;
}

static void hostfs_release(struct vnode *v)
{
    struct hnode *h = v->fs;

    if (h->handle >= 0) {
        host_call(OP_CLOSE, h->handle, 0, 0, 0);
    }
    kfree(h);
}

static const struct vnode_ops hostfs_ops = {
    hostfs_lookup, hostfs_read, hostfs_write, hostfs_readdir, hostfs_create,
    hostfs_unlink, NULL, hostfs_release, hostfs_truncate
};

/* The root vnode, or NULL when nothing answers on the other side. */
struct vnode *hostfs_init(void)
{
    if (host_call(OP_PROBE, 0, 0, 0, 0) != 0) {
        return NULL;
    }
    return hnode_vnode("", V_DIR, 0);
}
