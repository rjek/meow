/* The virtual file system: vnodes behind a table of mounts, open files,
   and a descriptor table.  Paths are absolute; the longest mount prefix
   wins and the rest of the path is walked in that file system a name at
   a time. */
#include "kernel.h"

#define NMOUNT 8

struct mount {
    const char *path;                   /* "/" or "/dev": no trailing slash */
    size_t len;
    struct vnode *root;
};

static struct mount mounts[NMOUNT];
static unsigned nmounts;
#define fds (current_process()->fds)

struct vnode *vnode_new(const struct vnode_ops *ops, int type, void *fs,
                        uint32_t ino, uint32_t size)
{
    struct vnode *v = kmalloc(sizeof *v);

    if (v == NULL) {
        return NULL;
    }
    v->ops = ops;
    v->type = type;
    v->fs = fs;
    v->ino = ino;
    v->size = size;
    v->refs = 1;
    return v;
}

void vnode_get(struct vnode *v)
{
    v->refs++;
}

void vnode_put(struct vnode *v)
{
    if (--v->refs == 0) {
        if (v->ops->release != NULL) {
            v->ops->release(v);
        }
        kfree(v);
    }
}

int vfs_mount(const char *path, struct vnode *root)
{
    struct mount *m;

    if (nmounts == NMOUNT) {
        return -ENOSPC;
    }
    m = &mounts[nmounts++];
    m->path = path;
    m->len = strlen(path);
    m->root = root;
    return 0;
}

static struct mount *mount_for(const char *path, const char **rest)
{
    struct mount *best = NULL;
    unsigned i;

    for (i = 0; i < nmounts; i++) {
        struct mount *m = &mounts[i];

        if (m->len == 1) {              /* "/" matches everything */
            if (best == NULL) {
                best = m;
            }
            continue;
        }
        if (strncmp(path, m->path, m->len) == 0 &&
            (path[m->len] == '/' || path[m->len] == '\0') &&
            (best == NULL || m->len > best->len)) {
            best = m;
        }
    }
    if (best == NULL) {
        return NULL;
    }
    *rest = path + (best->len == 1 ? 0 : best->len);
    return best;
}

/* Walk to the vnode a path names, with a reference on it. */
int vfs_lookup(const char *path, struct vnode **out)
{
    const char *rest;
    struct mount *m;
    struct vnode *v;

    if (path[0] != '/') {
        return -EINVAL;
    }
    m = mount_for(path, &rest);
    if (m == NULL) {
        return -ENOENT;
    }
    v = m->root;
    vnode_get(v);
    while (*rest != '\0') {
        char name[NAME_MAX + 1];
        size_t n = 0;
        struct vnode *next;
        int rc;

        while (*rest == '/') {
            rest++;
        }
        if (*rest == '\0') {
            break;
        }
        while (*rest != '\0' && *rest != '/') {
            if (n == NAME_MAX) {
                vnode_put(v);
                return -ENAMETOOLONG;
            }
            name[n++] = *rest++;
        }
        name[n] = '\0';
        if (v->type != V_DIR) {
            vnode_put(v);
            return -ENOTDIR;
        }
        if (v->ops->lookup == NULL) {
            vnode_put(v);
            return -ENOENT;
        }
        rc = v->ops->lookup(v, name, &next);
        vnode_put(v);
        if (rc < 0) {
            return rc;
        }
        v = next;
    }
    *out = v;
    return 0;
}

static int fd_alloc(struct file *f)
{
    int i;

    for (i = 0; i < NFD; i++) {
        if (fds[i] == NULL) {
            fds[i] = f;
            return i;
        }
    }
    return -EMFILE;
}

static struct file *fd_get(int fd)
{
    if (fd < 0 || fd >= NFD) {
        return NULL;
    }
    return fds[fd];
}

int vfs_open(const char *path, int flags)
{
    struct vnode *v;
    struct file *f;
    int rc;

    kenter();
    rc = vfs_lookup(path, &v);
    if (rc < 0) {
        kexit();
        return rc;
    }
    if (v->type == V_DIR && (flags & O_WRONLY) != 0) {
        vnode_put(v);
        kexit();
        return -EISDIR;
    }
    f = kmalloc(sizeof *f);
    if (f == NULL) {
        vnode_put(v);
        kexit();
        return -ENOMEM;
    }
    f->v = v;
    f->off = 0;
    f->flags = flags;
    f->refs = 1;
    rc = fd_alloc(f);
    if (rc < 0) {
        vnode_put(v);
        kfree(f);
    }
    kexit();
    return rc;
}

int vfs_close(int fd)
{
    struct file *f = fd_get(fd);

    if (f == NULL) {
        return -EBADF;
    }
    kenter();
    fds[fd] = NULL;
    if (--f->refs == 0) {
        vnode_put(f->v);
        kfree(f);
    }
    kexit();
    return 0;
}

int vfs_read(int fd, void *buf, size_t len)
{
    struct file *f = fd_get(fd);
    int n;

    if (f == NULL) {
        return -EBADF;
    }
    if (f->v->type == V_DIR) {
        return -EISDIR;
    }
    if (f->v->ops->read == NULL) {
        return -EINVAL;
    }
    n = f->v->ops->read(f->v, buf, len, f->off);
    if (n > 0) {
        f->off += (uint32_t)n;
    }
    return n;
}

int vfs_write(int fd, const void *buf, size_t len)
{
    struct file *f = fd_get(fd);
    int n;

    if (f == NULL) {
        return -EBADF;
    }
    if (f->v->ops->write == NULL) {
        return -EROFS;
    }
    n = f->v->ops->write(f->v, buf, len, f->off);
    if (n > 0) {
        f->off += (uint32_t)n;
    }
    return n;
}

int vfs_seek(int fd, int32_t off, int whence)
{
    struct file *f = fd_get(fd);
    int32_t base;

    if (f == NULL) {
        return -EBADF;
    }
    if (f->v->type != V_FILE) {
        return -ESPIPE;
    }
    switch (whence) {
    case SEEK_SET: base = 0; break;
    case SEEK_CUR: base = (int32_t)f->off; break;
    case SEEK_END: base = (int32_t)f->v->size; break;
    default: return -EINVAL;
    }
    if (base + off < 0) {
        return -EINVAL;
    }
    f->off = (uint32_t)(base + off);
    return (int)f->off;
}

/* The next entry of an open directory; 0 at the end. */
int vfs_readdir(int fd, struct dirent *de)
{
    struct file *f = fd_get(fd);
    int rc;

    if (f == NULL) {
        return -EBADF;
    }
    if (f->v->type != V_DIR || f->v->ops->readdir == NULL) {
        return -ENOTDIR;
    }
    rc = f->v->ops->readdir(f->v, f->off, de);
    if (rc > 0) {
        f->off++;
    }
    return rc;
}

int vfs_stat(const char *path, struct stat *st)
{
    struct vnode *v;
    int rc;

    kenter();
    rc = vfs_lookup(path, &v);
    if (rc == 0) {
        st->type = v->type;
        st->size = v->size;
        st->ino = v->ino;
        vnode_put(v);
    }
    kexit();
    return rc;
}

int vfs_ioctl(int fd, int req, void *arg)
{
    struct file *f = fd_get(fd);

    if (f == NULL) {
        return -EBADF;
    }
    if (f->v->ops->ioctl == NULL) {
        return -ENOTTY;
    }
    return f->v->ops->ioctl(f->v, req, arg);
}
