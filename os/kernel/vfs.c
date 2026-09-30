/* The virtual file system: vnodes behind a table of mounts, open files,
   and a descriptor table.  Paths are absolute; the longest mount prefix
   wins and the rest of the path is walked in that file system a name at
   a time. */
#include "kernel.h"

#define NMOUNT 12

struct mount {
    const char *path;                   /* "/" or "/dev": no trailing slash */
    size_t len;
    struct vnode *root;
    const char *type;                   /* the file system's name, for /proc/mounts */
    void *owner;                        /* the port that serves it, or NULL for the kernel's */
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

int vfs_mount_info(int index, const char **path, const char **type)
{
    if (index < 0 || (unsigned)index >= nmounts) {
        return 0;
    }
    *path = mounts[index].path;
    *type = mounts[index].type;
    return 1;
}

int vfs_mount(const char *path, struct vnode *root, const char *type)
{
    struct mount *m;

    if (nmounts == NMOUNT) {
        return -ENOSPC;
    }
    m = &mounts[nmounts++];
    m->path = path;
    m->len = strlen(path);
    m->root = root;
    m->type = type;
    m->owner = NULL;
    return 0;
}

static int normalise(const char *path, char *out);

/* A mount that a port serves, and that goes when the port does: the
   path may be relative, and is copied. */
int vfs_mount_owned(const char *path, struct vnode *root, const char *type, void *owner)
{
    char full[PATH_MAX], *copy;
    unsigned i;
    int rc = normalise(path, full);

    if (rc < 0) {
        return rc;
    }
    for (i = 0; i < nmounts; i++) {
        if (strcmp(mounts[i].path, full) == 0) {
            return -EEXIST;
        }
    }
    /* the path and the type in one block, since the server's name that
       the type is will not outlive the server */
    copy = kmalloc(strlen(full) + 1 + strlen(type) + 1);
    if (copy == NULL) {
        return -ENOMEM;
    }
    strcpy(copy, full);
    strcpy(copy + strlen(full) + 1, type);
    rc = vfs_mount(copy, root, copy + strlen(full) + 1);
    if (rc < 0) {
        kfree(copy);
        return rc;
    }
    mounts[nmounts - 1].owner = owner;
    return 0;
}

void vfs_umount_owner(void *owner)
{
    unsigned i, kept = 0;

    for (i = 0; i < nmounts; i++) {
        if (mounts[i].owner == owner) {
            struct vnode *root = mounts[i].root;

            kfree((char *)mounts[i].path);
            mounts[i].owner = NULL;
            vnode_put(root);
        } else {
            mounts[kept++] = mounts[i];
        }
    }
    nmounts = kept;
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

/* An absolute path with no ".", ".." or doubled slashes, from a path
   that may be relative to the working directory. */
static int normalise(const char *path, char *out)
{
    const char *cwd = current_process()->cwd;
    char *w = out;
    size_t len = 0;

    if (path[0] != '/' && strcmp(cwd, "/") != 0) {
        len = strlen(cwd);
        memcpy(out, cwd, len);
        w = out + len;
    }
    while (*path != '\0') {
        const char *start;
        size_t n;

        while (*path == '/') {
            path++;
        }
        if (*path == '\0') {
            break;
        }
        start = path;
        while (*path != '\0' && *path != '/') {
            path++;
        }
        n = (size_t)(path - start);
        if (n == 1 && start[0] == '.') {
            continue;
        }
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            while (w > out && *--w != '/') {
            }
            continue;
        }
        if ((size_t)(w - out) + n + 2 > PATH_MAX) {
            return -ENAMETOOLONG;
        }
        *w++ = '/';
        memcpy(w, start, n);
        w += n;
    }
    if (w == out) {
        *w++ = '/';
    }
    *w = '\0';
    return 0;
}

/* Split a path into its directory and last name. */
static int split(const char *path, char *dir, char *name)
{
    char full[PATH_MAX];
    char *slash;
    int rc = normalise(path, full);

    if (rc < 0) {
        return rc;
    }
    slash = strrchr(full, '/');
    if (slash[1] == '\0') {
        return -EINVAL;                 /* the root has no name */
    }
    if (strlen(slash + 1) > NAME_MAX) {
        return -ENAMETOOLONG;
    }
    strcpy(name, slash + 1);
    if (slash == full) {
        strcpy(dir, "/");
    } else {
        memcpy(dir, full, (size_t)(slash - full));
        dir[slash - full] = '\0';
    }
    return 0;
}

/* Walk to the vnode a path names, with a reference on it. */
int vfs_lookup(const char *path, struct vnode **out)
{
    char full[PATH_MAX];
    const char *rest;
    struct mount *m;
    struct vnode *v;
    int rc = normalise(path, full);

    if (rc < 0) {
        return rc;
    }
    path = full;
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

struct vnode *vfs_fd_vnode(int fd)
{
    struct file *f = fd_get(fd);

    return f == NULL ? NULL : f->v;
}

/* A descriptor on a vnode the caller holds a reference to; the file
   takes the reference over. */
int vfs_open_vnode(struct vnode *v, int flags)
{
    struct file *f = kmalloc(sizeof *f);
    int rc;

    if (f == NULL) {
        return -ENOMEM;
    }
    memset(f, 0, sizeof *f);
    f->v = v;
    f->off = (flags & O_APPEND) != 0 ? v->size : 0;
    f->flags = flags;
    f->refs = 1;
    rc = fd_alloc(f);
    if (rc < 0) {
        kfree(f);
    }
    return rc;
}

/* Make a file or directory in a directory that exists. */
static int create(const char *path, int type, struct vnode **out)
{
    char dir[PATH_MAX], name[NAME_MAX + 1];
    struct vnode *d;
    int rc = split(path, dir, name);

    if (rc < 0) {
        return rc;
    }
    rc = vfs_lookup(dir, &d);
    if (rc < 0) {
        return rc;
    }
    if (d->type != V_DIR) {
        rc = -ENOTDIR;
    } else if (d->ops->create == NULL) {
        rc = -EROFS;
    } else {
        rc = d->ops->create(d, name, type, out);
    }
    vnode_put(d);
    return rc;
}

int vfs_open(const char *path, int flags)
{
    struct vnode *v;
    int rc;

    kenter();
    rc = vfs_lookup(path, &v);
    if (rc == -ENOENT && (flags & O_CREAT) != 0) {
        rc = create(path, V_FILE, &v);
    }
    if (rc < 0) {
        kexit();
        return rc;
    }
    if (v->type == V_DIR && (flags & (O_WRONLY | O_RDWR)) != 0) {
        vnode_put(v);
        kexit();
        return -EISDIR;
    }
    if ((flags & O_TRUNC) != 0 && v->type == V_FILE) {
        if (v->ops->truncate == NULL) {
            vnode_put(v);
            kexit();
            return -EROFS;
        }
        v->ops->truncate(v);
    }
    rc = vfs_open_vnode(v, flags);
    if (rc < 0) {
        vnode_put(v);
    } else if (v->type == V_DIR) {
        char full[PATH_MAX];
        struct file *f = fds[rc];

        normalise(path, full);          /* it resolved, so it normalises */
        f->path = kmalloc(strlen(full) + 1);
        if (f->path == NULL) {
            vfs_close(rc);
            kexit();
            return -ENOMEM;
        }
        strcpy(f->path, full);
    }
    kexit();
    return rc;
}

int vfs_dup(int fd)
{
    struct file *f = fd_get(fd);
    int rc;

    if (f == NULL) {
        return -EBADF;
    }
    kenter();
    rc = fd_alloc(f);
    if (rc >= 0) {
        f->refs++;
    }
    kexit();
    return rc;
}

int vfs_dup2(int fd, int to)
{
    struct file *f = fd_get(fd);

    if (f == NULL || to < 0 || to >= NFD) {
        return -EBADF;
    }
    if (to == fd) {
        return to;
    }
    kenter();
    if (fds[to] != NULL) {
        vfs_close(to);
    }
    fds[to] = f;
    f->refs++;
    kexit();
    return to;
}

int vfs_mkdir(const char *path)
{
    struct vnode *v;
    int rc;

    kenter();
    rc = create(path, V_DIR, &v);
    if (rc == 0) {
        vnode_put(v);
    }
    kexit();
    return rc;
}

int vfs_unlink(const char *path)
{
    char dir[PATH_MAX], name[NAME_MAX + 1];
    struct vnode *d;
    int rc;

    kenter();
    rc = split(path, dir, name);
    if (rc == 0) {
        rc = vfs_lookup(dir, &d);
    }
    if (rc == 0) {
        rc = d->ops->unlink == NULL ? -EROFS : d->ops->unlink(d, name);
        vnode_put(d);
    }
    kexit();
    return rc;
}

int vfs_chdir(const char *path)
{
    char full[PATH_MAX];
    struct vnode *v;
    int rc;

    kenter();
    rc = normalise(path, full);
    if (rc == 0) {
        rc = vfs_lookup(full, &v);
    }
    if (rc == 0) {
        rc = v->type == V_DIR ? 0 : -ENOTDIR;
        vnode_put(v);
    }
    if (rc == 0) {
        strcpy(current_process()->cwd, full);
    }
    kexit();
    return rc;
}

int vfs_getcwd(char *buf, size_t size)
{
    const char *cwd = current_process()->cwd;

    if (strlen(cwd) + 1 > size) {
        return -ENAMETOOLONG;
    }
    strcpy(buf, cwd);
    return 0;
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
        if (f->v->type == V_PIPE) {
            pipe_end_closed(f->v, f->flags);
        }
        vnode_put(f->v);
        kfree(f->path);
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
    if ((f->flags & O_APPEND) != 0) {
        f->off = f->v->size;
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

/* The name of a mount point directly inside dir, or NULL if m is not
   one: "/dev" is in "/", "/host/x" is in "/host", "/" is in nothing. */
static const char *mount_in(const struct mount *m, const char *dir)
{
    size_t len = strlen(dir);
    const char *name;

    if (m->len == 1) {
        return NULL;
    }
    if (len == 1) {
        name = m->path + 1;
    } else if (strncmp(m->path, dir, len) == 0 && m->path[len] == '/') {
        name = m->path + len + 1;
    } else {
        return NULL;
    }
    return strchr(name, '/') == NULL ? name : NULL;
}

/* Whether the directory's own file system lists name */
static int fs_lists(struct file *f, const char *name)
{
    struct dirent de;
    uint32_t i;

    for (i = 0; i < f->fs_entries; i++) {
        if (f->v->ops->readdir(f->v, i, &de) > 0 && strcmp(de.name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* The next entry of an open directory; 0 at the end.  First what its
   file system holds, then the mount points in it that the file system
   does not already list, since those live only in the mount table. */
int vfs_readdir(int fd, struct dirent *de)
{
    struct file *f = fd_get(fd);
    uint32_t k;
    unsigned i;
    int rc;

    if (f == NULL) {
        return -EBADF;
    }
    if (f->v->type != V_DIR) {
        return -ENOTDIR;
    }
    kenter();
    if (f->fs_done == 0) {
        rc = f->v->ops->readdir == NULL ? 0 : f->v->ops->readdir(f->v, f->off, de);
        if (rc != 0) {
            if (rc > 0) {
                f->off++;
            }
            kexit();
            return rc;
        }
        f->fs_entries = f->off;
        f->fs_done = 1;
    }
    k = f->off - f->fs_entries;
    for (i = 0; f->path != NULL && i < nmounts; i++) {
        const char *name = mount_in(&mounts[i], f->path);

        if (name == NULL || fs_lists(f, name)) {
            continue;
        }
        if (k-- == 0) {
            strncpy_(de->name, name, sizeof de->name);
            de->type = V_DIR;
            de->size = 0;
            f->off++;
            kexit();
            return 1;
        }
    }
    kexit();
    return 0;
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

/* ---- poll: which of these descriptors can be read or written without
   waiting.  There is one queue for everyone who is waiting to know, and
   whatever changes the answer for any file wakes them all to look
   again; at this scale that is cheaper than a list on every file. ---- */

static struct waitq pollq;

/* Something became readable or writable.  Returns the most urgent
   thread woken, for a caller that may want to make way for it. */
struct thread *poll_wake(void)
{
    struct thread *t, *best = NULL;

    while ((t = waitq_wake_one(&pollq)) != NULL) {
        if (best == NULL || t->prio > best->prio) {
            best = t;
        }
    }
    return best;
}

/* Fill in revents for each of n descriptors and return how many have
   any, waiting up to ticks for the first: 0 does not wait, a negative
   number waits for ever. */
int vfs_poll(struct pollfd *p, unsigned n, int ticks)
{
    uint32_t end = ticks_now() + (uint32_t)ticks;
    unsigned i;
    int count;

    kenter();
    for (;;) {
        count = 0;
        for (i = 0; i < n; i++) {
            struct file *f = fd_get(p[i].fd);
            int m = POLLNVAL;

            if (f != NULL) {
                m = f->v->ops->poll == NULL ? POLLIN | POLLOUT : f->v->ops->poll(f->v);
                m &= p[i].events | POLLERR | POLLHUP;
            }
            p[i].revents = (short)m;
            if (m != 0) {
                count++;
            }
        }
        if (count != 0 || ticks == 0) {
            break;
        }
        if (ticks < 0) {
            waitq_wait(&pollq);
        } else {
            int32_t left = (int32_t)(end - ticks_now());

            if (left <= 0 || waitq_wait_for(&pollq, (uint32_t)left) == 0) {
                ticks = 0;              /* one last look */
            }
        }
    }
    kexit();
    return count;
}
