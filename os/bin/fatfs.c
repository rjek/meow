/* fatfs: a FAT16 or FAT32 volume on a block device, mounted where its
   second argument says.  Run it in the background:

       fatfs /dev/sd0 /sd &

   Short names only, 8.3, shown in lower case and matched without
   regard to case; long-name entries are passed over, so a file given
   a long name elsewhere is seen by its short one.  Files are read,
   written and made, directories too, and both removed.  Times are not
   kept.  The volume may be the whole device or the first partition.

   The server keeps no state but two sectors: a file is known to the
   kernel by where its directory entry is, sector and slot, and
   everything about it is read from there when asked.  Small, and it
   survives the card being pulled and put back. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "catflap.h"

#define SEC         512
#define NCACHE      2
#define ROOT        0                   /* the root directory's node */
#define ATTR_DIR    0x10
#define ATTR_FILE   0x20
#define ATTR_LFN    0x0f
#define DATE_1980   0x0021

static int dev;
static unsigned part;                   /* first sector of the volume */
static unsigned spc, fat_start, nfats, fat_sectors, root_start, root_sectors;
static unsigned data_start, root_cluster, nclusters;
static int fat32;

/* ---- sectors, through a small cache ---- */

struct sec {
    unsigned n, age;
    int valid, dirty;
    unsigned char d[SEC];
};

static struct sec cache[NCACHE];
static unsigned ticks;

static int devio(unsigned n, unsigned char *d, int writing)
{
    int rc;

    if (vfs_seek(dev, (int)(n * SEC), CF_SEEK_SET) < 0) {
        return -EIO;
    }
    rc = writing != 0 ? vfs_write(dev, d, SEC) : vfs_read(dev, d, SEC);
    return rc == SEC ? 0 : -EIO;
}

static int flush(struct sec *s)
{
    int rc = 0;

    if (s->valid != 0 && s->dirty != 0) {
        rc = devio(s->n, s->d, 1);
        s->dirty = 0;
    }
    return rc;
}

/* Sector n of the device, in the cache; NULL if it cannot be read.  The
   pointer is good until the next call: copy out what is wanted. */
static unsigned char *sector(unsigned n)
{
    struct sec *s, *old = &cache[0];

    for (s = cache; s < cache + NCACHE; s++) {
        if (s->valid != 0 && s->n == n) {
            s->age = ++ticks;
            return s->d;
        }
        if (s->age < old->age) {
            old = s;
        }
    }
    if (flush(old) < 0 || devio(n, old->d, 0) < 0) {
        old->valid = 0;
        return NULL;
    }
    old->n = n;
    old->valid = 1;
    old->age = ++ticks;
    return old->d;
}

static void dirty(unsigned n)
{
    struct sec *s;

    for (s = cache; s < cache + NCACHE; s++) {
        if (s->valid != 0 && s->n == n) {
            s->dirty = 1;
        }
    }
}

static int sync_all(void)
{
    int rc = 0;
    struct sec *s;

    for (s = cache; s < cache + NCACHE; s++) {
        if (flush(s) < 0) {
            rc = -EIO;
        }
    }
    return rc;
}

static unsigned rd16(const unsigned char *p)
{
    return p[0] | ((unsigned)p[1] << 8);
}

static unsigned rd32(const unsigned char *p)
{
    return rd16(p) | (rd16(p + 2) << 16);
}

static void wr16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void wr32(unsigned char *p, unsigned v)
{
    wr16(p, v & 0xffff);
    wr16(p + 2, v >> 16);
}

/* ---- the FAT and clusters ---- */

static unsigned eoc(void)
{
    return fat32 != 0 ? 0x0fffffff : 0xffff;
}

static int is_end(unsigned c)
{
    return c < 2 || c >= (fat32 != 0 ? 0x0ffffff8 : 0xfff8);
}

static unsigned fat_get(unsigned c)
{
    unsigned off = c * (fat32 != 0 ? 4 : 2);
    unsigned char *p = sector(part + fat_start + off / SEC);

    if (p == NULL) {
        return 1;                       /* reads as an end */
    }
    p += off % SEC;
    return fat32 != 0 ? rd32(p) & 0x0fffffff : rd16(p);
}

static int fat_set(unsigned c, unsigned v)
{
    unsigned off = c * (fat32 != 0 ? 4 : 2), f;

    for (f = 0; f < nfats; f++) {
        unsigned n = part + fat_start + f * fat_sectors + off / SEC;
        unsigned char *p = sector(n);

        if (p == NULL) {
            return -EIO;
        }
        if (fat32 != 0) {
            wr32(p + off % SEC, v);
        } else {
            wr16(p + off % SEC, v);
        }
        dirty(n);
    }
    return 0;
}

static unsigned cluster_sector(unsigned c)
{
    return part + data_start + (c - 2) * spc;
}

/* A free cluster, zeroed, chained after prev if there is one; 0 if none */
static unsigned alloc_cluster(unsigned prev)
{
    unsigned c, k;

    for (c = 2; c < nclusters + 2; c++) {
        if (fat_get(c) == 0) {
            for (k = 0; k < spc; k++) {
                unsigned char *p = sector(cluster_sector(c) + k);

                if (p == NULL) {
                    return 0;
                }
                memset(p, 0, SEC);
                dirty(cluster_sector(c) + k);
            }
            if (fat_set(c, eoc()) < 0 || (prev != 0 && fat_set(prev, c) < 0)) {
                return 0;
            }
            return c;
        }
    }
    return 0;
}

static void free_chain(unsigned c)
{
    while (is_end(c) == 0) {
        unsigned next = fat_get(c);

        fat_set(c, 0);
        c = next;
    }
}

/* ---- directory entries ---- */

static unsigned char *entry(unsigned node)
{
    unsigned char *p = sector(node >> 4);

    return p != NULL ? p + (node & 15) * 32 : NULL;
}

static unsigned entry_cluster(const unsigned char *e)
{
    return (fat32 != 0 ? rd16(e + 20) << 16 : 0) | rd16(e + 26);
}

static void set_cluster(unsigned char *e, unsigned c)
{
    wr16(e + 26, c & 0xffff);
    if (fat32 != 0) {
        wr16(e + 20, c >> 16);
    }
}

/* What a node is: its first cluster, size and whether a directory */
static int describe(unsigned node, unsigned *cluster, unsigned *size, int *dir)
{
    unsigned char *e;

    if (node == ROOT) {
        *cluster = fat32 != 0 ? root_cluster : 0;
        *size = 0;
        *dir = 1;
        return 0;
    }
    e = entry(node);
    if (e == NULL) {
        return -EIO;
    }
    *cluster = entry_cluster(e);
    *size = rd32(e + 28);
    *dir = (e[11] & ATTR_DIR) != 0;
    return 0;
}

/* The k-th sector of a directory: 1 and its number, 0 past the end */
static int dir_sector(unsigned node, unsigned k, unsigned *out)
{
    unsigned c, size;
    int dir;

    if (node == ROOT && fat32 == 0) {
        *out = part + root_start + k;
        return k < root_sectors;
    }
    if (describe(node, &c, &size, &dir) < 0) {
        return 0;
    }
    for (k += 0; k >= spc && is_end(c) == 0; k -= spc) {
        c = fat_get(c);
    }
    if (is_end(c) != 0) {
        return 0;
    }
    *out = cluster_sector(c) + k;
    return 1;
}

/* Name to the 11 bytes of an entry; -1 if it will not fit */
static int to83(const char *name, unsigned char *out)
{
    const char *dot = strrchr(name, '.');
    unsigned n = dot != NULL ? (unsigned)(dot - name) : (unsigned)strlen(name), i;

    memset(out, ' ', 11);
    if (n == 0 || n > 8 || (dot != NULL && strlen(dot + 1) > 3) || name[0] == '.') {
        return -1;
    }
    for (i = 0; i < n + (dot != NULL ? 1 + strlen(dot + 1) : 0); i++) {
        char c = name[i];

        if (c == ' ' || c == '/' || c == '"' || c == '*' || c == '?' || c == '|' || c == '\\' || c < ' ') {
            return -1;
        }
        if (i != n) {
            out[i < n ? i : 8 + i - n - 1] = (unsigned char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
    }
    return 0;
}

static void from83(const unsigned char *e, char *out)
{
    int i, n = 0;

    for (i = 0; i < 11; i++) {
        if (i == 8 && e[8] != ' ') {
            out[n++] = '.';
        }
        if (e[i] != ' ') {
            out[n++] = (char)(e[i] >= 'A' && e[i] <= 'Z' ? e[i] + 32 : e[i]);
        }
    }
    out[n] = '\0';
}

/* Walk a directory: for a name, the entry with it; for an index, the
   index-th listed entry (not . or ..); for neither, a free slot.
   Returns 1 and the node, 0 if there is none, or an error. */
static int walk(unsigned dir, const unsigned char *name, int index, unsigned *node)
{
    unsigned k, s, i;

    for (k = 0; dir_sector(dir, k, &s) != 0; k++) {
        for (i = 0; i < SEC / 32; i++) {
            unsigned char *e = sector(s);
            unsigned char first;

            if (e == NULL) {
                return -EIO;
            }
            e += i * 32;
            first = e[0];
            if (first == 0 || first == 0xe5) {
                if (name == NULL && index < 0) {
                    *node = (s << 4) | i;
                    return 1;
                }
                if (first == 0) {
                    return 0;
                }
                continue;
            }
            if ((e[11] & 0x08) != 0 || (e[11] & ATTR_LFN) == ATTR_LFN || first == '.') {
                continue;               /* a label, a long name, or . and .. */
            }
            if ((name != NULL && memcmp(e, name, 11) == 0) ||
                (name == NULL && index-- == 0)) {
                *node = (s << 4) | i;
                return 1;
            }
        }
    }
    return 0;
}

/* ---- the operations ---- */

static int lookup(unsigned dir, struct cf_req *r)
{
    unsigned char name[11];
    unsigned node, cluster, size;
    int rc, isdir;

    if (to83(r->buf, name) < 0) {
        return -ENOENT;
    }
    rc = walk(dir, name, -1, &node);
    if (rc != 1) {
        return rc < 0 ? rc : -ENOENT;
    }
    if (describe(node, &cluster, &size, &isdir) < 0) {
        return -EIO;
    }
    r->new_node = node;
    r->type = isdir != 0 ? CF_V_DIR : CF_V_FILE;
    r->size = size;
    return 0;
}

static int readdir_(unsigned dir, struct cf_req *r)
{
    struct cf_dirent *de = r->buf;
    unsigned node, cluster;
    int rc, isdir;
    unsigned char *e;

    rc = walk(dir, NULL, (int)r->off, &node);
    if (rc != 1) {
        return rc;
    }
    e = entry(node);
    if (e == NULL) {
        return -EIO;
    }
    from83(e, de->name);
    if (describe(node, &cluster, &de->size, &isdir) < 0) {
        return -EIO;
    }
    de->type = isdir != 0 ? CF_V_DIR : CF_V_FILE;
    return 1;
}

/* The cluster after c; for a write, a new one when the chain ends.  0
   when there is none. */
static unsigned step(unsigned c, int writing)
{
    unsigned next = fat_get(c);

    if (is_end(next) != 0) {
        return writing != 0 ? alloc_cluster(c) : 0;
    }
    return next;
}

/* Bytes of a file at off: read into, or written from, r->buf, the
   chain extended as a write needs */
static int transfer(unsigned node, struct cf_req *r, int writing)
{
    unsigned cluster, size, c, skip, off = r->off, len = r->len, done = 0, csize = spc * SEC;
    unsigned char *e;
    int isdir;

    if (describe(node, &cluster, &size, &isdir) < 0) {
        return -EIO;
    }
    if (isdir != 0) {
        return -EISDIR;
    }
    if (writing == 0) {
        if (off >= size) {
            return 0;
        }
        if (len > size - off) {
            len = size - off;
        }
    } else if (cluster == 0 && len != 0) {
        cluster = alloc_cluster(0);
        if (cluster == 0) {
            return -ENOSPC;
        }
        e = entry(node);
        set_cluster(e, cluster);
        dirty(node >> 4);
    }
    c = cluster;
    for (skip = off / csize; skip > 0 && c != 0; skip--) {
        c = step(c, writing);
    }
    while (c != 0 && done < len) {
        unsigned at = off + done, n = cluster_sector(c) + (at % csize) / SEC;
        unsigned chunk = SEC - at % SEC;
        unsigned char *p = sector(n);

        if (chunk > len - done) {
            chunk = len - done;
        }
        if (p == NULL) {
            break;
        }
        if (writing != 0) {
            memcpy(p + at % SEC, (char *)r->buf + done, chunk);
            dirty(n);
        } else {
            memcpy((char *)r->buf + done, p + at % SEC, chunk);
        }
        done += chunk;
        if ((at + chunk) % csize == 0 && done < len) {
            c = step(c, writing);
        }
    }
    if (writing != 0) {
        if (off + done > size) {
            size = off + done;
            e = entry(node);
            wr32(e + 28, size);
            dirty(node >> 4);
        }
        r->size = size;
        if (sync_all() < 0) {
            return -EIO;
        }
    }
    return (int)done;
}

static int create(unsigned dir, struct cf_req *r)
{
    unsigned char name[11], *e;
    unsigned node, parent_cluster, size, c = 0;
    int rc, isdir, want_dir = r->off == CF_V_DIR;

    if (to83(r->buf, name) < 0) {
        return -ENAMETOOLONG;
    }
    rc = walk(dir, name, -1, &node);
    if (rc != 0) {
        return rc < 0 ? rc : -EEXIST;
    }
    rc = walk(dir, NULL, -1, &node);
    if (rc < 0) {
        return rc;
    }
    if (rc == 0) {                      /* full: one more cluster of it */
        unsigned last, next;

        if (describe(dir, &last, &size, &isdir) < 0 || last == 0) {
            return -ENOSPC;             /* the fixed root of FAT16 */
        }
        while (is_end(next = fat_get(last)) == 0) {
            last = next;
        }
        if (alloc_cluster(last) == 0 || walk(dir, NULL, -1, &node) != 1) {
            return -ENOSPC;
        }
    }
    if (want_dir != 0) {
        c = alloc_cluster(0);
        if (c == 0) {
            return -ENOSPC;
        }
        if (describe(dir, &parent_cluster, &size, &isdir) < 0) {
            return -EIO;
        }
        e = sector(cluster_sector(c));
        if (e == NULL) {
            return -EIO;
        }
        memset(e, ' ', 11);
        e[0] = '.';
        e[11] = ATTR_DIR;
        set_cluster(e, c);
        memset(e + 32, ' ', 11);
        e[32] = '.';
        e[33] = '.';
        e[32 + 11] = ATTR_DIR;
        set_cluster(e + 32, dir == ROOT ? 0 : parent_cluster);
        dirty(cluster_sector(c));
    }
    e = entry(node);
    if (e == NULL) {
        return -EIO;
    }
    memset(e, 0, 32);
    memcpy(e, name, 11);
    e[11] = (unsigned char)(want_dir != 0 ? ATTR_DIR : ATTR_FILE);
    wr16(e + 16, DATE_1980);
    wr16(e + 18, DATE_1980);
    wr16(e + 24, DATE_1980);
    set_cluster(e, c);
    dirty(node >> 4);
    r->new_node = node;
    return sync_all();
}

static int unlink_(unsigned dir, struct cf_req *r)
{
    unsigned char name[11], *e;
    unsigned node, cluster, size, other;
    int rc, isdir;

    if (to83(r->buf, name) < 0) {
        return -ENOENT;
    }
    rc = walk(dir, name, -1, &node);
    if (rc != 1) {
        return rc < 0 ? rc : -ENOENT;
    }
    if (describe(node, &cluster, &size, &isdir) < 0) {
        return -EIO;
    }
    if (isdir != 0 && walk(node, NULL, 0, &other) != 0) {
        return -ENOTEMPTY;
    }
    free_chain(cluster);
    e = entry(node);
    if (e == NULL) {
        return -EIO;
    }
    e[0] = 0xe5;
    dirty(node >> 4);
    return sync_all();
}

static int truncate_(unsigned node, struct cf_req *r)
{
    unsigned cluster, size;
    unsigned char *e;
    int isdir;

    if (describe(node, &cluster, &size, &isdir) < 0) {
        return -EIO;
    }
    if (isdir != 0) {
        return -EISDIR;
    }
    free_chain(cluster);
    e = entry(node);
    if (e == NULL) {
        return -EIO;
    }
    set_cluster(e, 0);
    wr32(e + 28, 0);
    dirty(node >> 4);
    r->size = 0;
    return sync_all();
}

static int serve(struct cf_req *r)
{
    switch (r->op) {
    case CF_OP_LOOKUP: return lookup(r->node, r);
    case CF_OP_READ: return transfer(r->node, r, 0);
    case CF_OP_WRITE: return transfer(r->node, r, 1);
    case CF_OP_READDIR: return readdir_(r->node, r);
    case CF_OP_CREATE: return create(r->node, r);
    case CF_OP_UNLINK: return unlink_(r->node, r);
    case CF_OP_TRUNCATE: return truncate_(r->node, r);
    default: return -ENOTTY;
    }
}

/* The boot sector, at the start of the device or of its first
   partition.  Returns 0, or -1 for something that is not a FAT. */
static int mount_volume(void)
{
    unsigned char *p = sector(0);
    unsigned total, root_entries, fs16;

    if (p == NULL) {
        return -1;
    }
    if (rd16(p + 11) != SEC && p[510] == 0x55 && p[511] == 0xaa) {
        part = rd32(p + 446 + 8);       /* a partition table: the first one */
        p = sector(part);
        if (p == NULL) {
            return -1;
        }
    }
    if (rd16(p + 11) != SEC || p[13] == 0 || (p[0] != 0xeb && p[0] != 0xe9)) {
        return -1;
    }
    spc = p[13];
    fat_start = rd16(p + 14);
    nfats = p[16];
    root_entries = rd16(p + 17);
    total = rd16(p + 19);
    if (total == 0) {
        total = rd32(p + 32);
    }
    fs16 = rd16(p + 22);
    fat32 = fs16 == 0;
    fat_sectors = fat32 != 0 ? rd32(p + 36) : fs16;
    root_cluster = fat32 != 0 ? rd32(p + 44) : 0;
    root_sectors = (root_entries * 32 + SEC - 1) / SEC;
    root_start = fat_start + nfats * fat_sectors;
    data_start = root_start + root_sectors;
    nclusters = (total - data_start) / spc;
    if (nclusters < 4085) {
        fprintf(stderr, "fatfs: FAT12 is not supported\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    struct cf_req *r;
    int port, rc, i;

    if (argc != 3) {
        fprintf(stderr, "usage: fatfs device directory &\n");
        return 2;
    }
    for (i = 0; i < 20; i++) {          /* the device's server may be starting too */
        dev = vfs_open(argv[1], CF_O_RDWR);
        if (dev >= 0 || dev != -ENOENT) {
            break;
        }
        thread_sleep(5);
    }
    if (dev < 0) {
        return 1;
    }
    if (mount_volume() < 0) {
        return 1;
    }
    port = srv_create(0, 0);
    rc = port < 0 ? port : srv_mount(port, argv[2], ROOT);
    if (rc < 0) {
        fprintf(stderr, "fatfs: cannot mount at %s: error %d\n", argv[2], -rc);
        return 1;
    }
    printf("fatfs: FAT%d, %u clusters of %u, at %s\n", fat32 != 0 ? 32 : 16, nclusters,
           spc * SEC, argv[2]);
    fflush(stdout);
    while (srv_recv(port, &r, -1) > 0) {
        srv_reply(port, r, serve(r));
    }
    sync_all();
    return 0;
}
