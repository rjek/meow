/* romfs: the read-only file system mkromfs packs into the ROM.  Entries
   are full paths sorted, so a lookup is a binary search for the parent's
   path plus the name, and a directory listing is a scan for entries one
   level below it. */
#include "kernel.h"

struct romfs_header {
    char magic[8];
    uint32_t nentries, names_size, data_size;
};

struct romfs_entry {
    uint32_t name_off;
    uint32_t data_off;                  /* 0xffffffff for a directory */
    uint32_t size;
    uint32_t reserved;
};

struct romfs {
    const struct romfs_header *h;
    const struct romfs_entry *e;
    const char *names;
    const unsigned char *data;
};

static struct romfs fs;

static const char *entry_path(const struct romfs *r, uint32_t i)
{
    return r->names + r->e[i].name_off;
}

static int is_dir(const struct romfs *r, uint32_t i)
{
    return r->e[i].data_off == 0xffffffffu;
}

static const struct vnode_ops romfs_ops;

/* The vnode's ino is the entry index plus one; 0 is the root, which has
   no entry of its own. */
static struct vnode *make_vnode(struct romfs *r, uint32_t i)
{
    if (i == 0) {
        return vnode_new(&romfs_ops, V_DIR, r, 0, 0);
    }
    return vnode_new(&romfs_ops, is_dir(r, i - 1) ? V_DIR : V_FILE, r, i,
                     is_dir(r, i - 1) ? 0 : r->e[i - 1].size);
}

static int find(const struct romfs *r, const char *path)
{
    int lo = 0, hi = (int)r->h->nentries - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(path, entry_path(r, (uint32_t)mid));

        if (c == 0) {
            return mid;
        }
        if (c < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return -1;
}

static int romfs_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    struct romfs *r = dir->fs;
    char path[PATH_MAX];
    const char *parent = dir->ino == 0 ? "" : entry_path(r, dir->ino - 1);
    int i;

    if (strlen(parent) + 1 + strlen(name) >= sizeof path) {
        return -ENAMETOOLONG;
    }
    strcpy(path, parent);
    if (parent[0] != '\0') {
        strcat(path, "/");
    }
    strcat(path, name);
    i = find(r, path);
    if (i < 0) {
        return -ENOENT;
    }
    *out = make_vnode(r, (uint32_t)i + 1);
    return *out == NULL ? -ENOMEM : 0;
}

static int romfs_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    struct romfs *r = v->fs;
    const struct romfs_entry *e = &r->e[v->ino - 1];

    if (off >= e->size) {
        return 0;
    }
    if (len > e->size - off) {
        len = e->size - off;
    }
    memcpy(buf, r->data + e->data_off + off, len);
    return (int)len;
}

/* Entries directly below a directory, in order, index-th one. */
static int romfs_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    struct romfs *r = v->fs;
    const char *parent = v->ino == 0 ? "" : entry_path(r, v->ino - 1);
    size_t plen = strlen(parent);
    uint32_t i, seen = 0;

    for (i = 0; i < r->h->nentries; i++) {
        const char *p = entry_path(r, i);
        const char *tail;

        if (plen != 0) {
            if (strncmp(p, parent, plen) != 0 || p[plen] != '/') {
                continue;
            }
            tail = p + plen + 1;
        } else {
            tail = p;
        }
        if (strchr(tail, '/') != NULL) {
            continue;                   /* deeper */
        }
        if (seen++ == index) {
            strcpy(de->name, tail);
            de->type = is_dir(r, i) ? V_DIR : V_FILE;
            de->size = is_dir(r, i) ? 0 : r->e[i].size;
            return 1;
        }
    }
    return 0;
}

static const struct vnode_ops romfs_ops = {
    romfs_lookup, romfs_read, NULL, romfs_readdir, NULL, NULL, NULL, NULL
};

/* The root vnode of the image in ROM, or NULL if there is none. */
struct vnode *romfs_init(const void *image)
{
    const struct romfs_header *h = image;

    if (memcmp(h->magic, "catflap1", 8) != 0) {
        return NULL;
    }
    fs.h = h;
    fs.e = (const struct romfs_entry *)(h + 1);
    fs.names = (const char *)(fs.e + h->nentries);
    fs.data = (const unsigned char *)fs.names + h->names_size;
    return make_vnode(&fs, 0);
}
