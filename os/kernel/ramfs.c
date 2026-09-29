/* ramfs: files and directories in the heap, for /tmp.  A node is a
   name, a kind, and either a growing buffer or a list of children. */
#include "kernel.h"

struct rnode {
    char name[NAME_MAX + 1];
    int type;
    struct rnode *next;                 /* sibling */
    struct rnode *children;
    char *data;
    uint32_t size, capacity;
    int links;                          /* 1 while in its directory, plus open vnodes */
};

static const struct vnode_ops ramfs_ops;

static struct vnode *node_vnode(struct rnode *n)
{
    struct vnode *v = vnode_new(&ramfs_ops, n->type, n, (uint32_t)(uintptr_t)n, n->size);

    if (v != NULL) {
        n->links++;
    }
    return v;
}

static int ramfs_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    struct rnode *d = dir->fs, *n;

    for (n = d->children; n != NULL; n = n->next) {
        if (strcmp(n->name, name) == 0) {
            *out = node_vnode(n);
            return *out == NULL ? -ENOMEM : 0;
        }
    }
    return -ENOENT;
}

static int ramfs_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    struct rnode *n = v->fs;

    if (off >= n->size) {
        return 0;
    }
    if (len > n->size - off) {
        len = n->size - off;
    }
    memcpy(buf, n->data + off, len);
    return (int)len;
}

static int grow(struct rnode *n, uint32_t need)
{
    uint32_t cap = n->capacity == 0 ? 64 : n->capacity;
    char *data;

    while (cap < need) {
        cap *= 2;
    }
    data = kmalloc(cap);
    if (data == NULL) {
        return -ENOSPC;
    }
    memcpy(data, n->data, n->size);
    kfree(n->data);
    n->data = data;
    n->capacity = cap;
    return 0;
}

static int ramfs_write(struct vnode *v, const void *buf, size_t len, uint32_t off)
{
    struct rnode *n = v->fs;

    if (off + len > n->capacity && grow(n, off + (uint32_t)len) < 0) {
        return -ENOSPC;
    }
    if (off > n->size) {
        memset(n->data + n->size, 0, off - n->size);
    }
    memcpy(n->data + off, buf, len);
    if (off + len > n->size) {
        n->size = off + (uint32_t)len;
    }
    v->size = n->size;
    return (int)len;
}

static int ramfs_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    struct rnode *n = ((struct rnode *)v->fs)->children;

    while (n != NULL && index > 0) {
        n = n->next;
        index--;
    }
    if (n == NULL) {
        return 0;
    }
    strcpy(de->name, n->name);
    de->type = n->type;
    de->size = n->size;
    return 1;
}

static int ramfs_create(struct vnode *dir, const char *name, int type, struct vnode **out)
{
    struct rnode *d = dir->fs, *n, **pp;

    for (n = d->children; n != NULL; n = n->next) {
        if (strcmp(n->name, name) == 0) {
            return -EEXIST;
        }
    }
    n = kmalloc(sizeof *n);
    if (n == NULL) {
        return -ENOSPC;
    }
    memset(n, 0, sizeof *n);
    strcpy(n->name, name);
    n->type = type;
    n->links = 1;
    for (pp = &d->children; *pp != NULL; pp = &(*pp)->next) {
    }
    *pp = n;                            /* at the end, so listings are in creation order */
    *out = node_vnode(n);
    return *out == NULL ? -ENOMEM : 0;
}

static void node_free(struct rnode *n)
{
    kfree(n->data);
    kfree(n);
}

static int ramfs_unlink(struct vnode *dir, const char *name)
{
    struct rnode *d = dir->fs, **pp;

    for (pp = &d->children; *pp != NULL; pp = &(*pp)->next) {
        struct rnode *n = *pp;

        if (strcmp(n->name, name) != 0) {
            continue;
        }
        if (n->type == V_DIR && n->children != NULL) {
            return -ENOTEMPTY;
        }
        *pp = n->next;
        if (--n->links == 0) {
            node_free(n);
        }
        return 0;
    }
    return -ENOENT;
}

static int ramfs_truncate(struct vnode *v)
{
    struct rnode *n = v->fs;

    n->size = 0;
    v->size = 0;
    return 0;
}

static void ramfs_release(struct vnode *v)
{
    struct rnode *n = v->fs;

    if (--n->links == 0) {
        node_free(n);                   /* unlinked while open */
    }
}

static const struct vnode_ops ramfs_ops = {
    ramfs_lookup, ramfs_read, ramfs_write, ramfs_readdir, ramfs_create,
    ramfs_unlink, NULL, ramfs_release, ramfs_truncate
};

struct vnode *ramfs_init(void)
{
    struct rnode *root = kmalloc(sizeof *root);

    if (root == NULL) {
        return NULL;
    }
    memset(root, 0, sizeof *root);
    root->type = V_DIR;
    root->links = 1;
    return node_vnode(root);
}
