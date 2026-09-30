/* memfs: a file system held in this program's memory, mounted where its
   argument says.  It is what a file system server looks like: a port, a
   mount, and a loop that answers requests until the port is closed by
   the program's end, by kill or otherwise.  Run it in the background:

       memfs /mnt &

   A file is a node, and a node's number, which is all the kernel knows
   it by, is its slot in a table together with a count of how often the
   slot has been used, so that a number held for a file since removed
   is not taken for whatever has the slot now. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catflap.h"

#define NODES 64
#define ROOT 1

struct node {
    char name[32];
    int used;
    int type;                           /* CF_V_FILE or CF_V_DIR */
    unsigned parent;                    /* slot of its directory */
    unsigned uses;
    char *data;
    unsigned size, capacity;
};

static struct node nodes[NODES];

static unsigned number(const struct node *n)
{
    return (n->uses << 8) | (unsigned)(n - nodes);
}

static struct node *node(unsigned number)
{
    struct node *n = &nodes[number & 0xff];

    return (number & 0xff) < NODES && n->used != 0 && n->uses == number >> 8 ? n : NULL;
}

static struct node *child(const struct node *dir, const char *name)
{
    unsigned i;

    for (i = 0; i < NODES; i++) {
        if (nodes[i].used != 0 && &nodes[i] != dir &&
            nodes[i].parent == (unsigned)(dir - nodes) && strcmp(nodes[i].name, name) == 0) {
            return &nodes[i];
        }
    }
    return NULL;
}

static int create(struct node *dir, const char *name, int type, struct cf_req *r)
{
    unsigned i;

    if (child(dir, name) != NULL) {
        return -EEXIST;
    }
    for (i = 0; i < NODES; i++) {
        if (nodes[i].used == 0) {
            struct node *n = &nodes[i];

            strncpy(n->name, name, sizeof n->name - 1);
            n->name[sizeof n->name - 1] = '\0';
            n->used = 1;
            n->type = type;
            n->parent = (unsigned)(dir - nodes);
            n->uses++;
            r->new_node = number(n);
            return 0;
        }
    }
    return -ENOSPC;
}

static int unlink_(struct node *dir, const char *name)
{
    struct node *n = child(dir, name);
    unsigned i;

    if (n == NULL) {
        return -ENOENT;
    }
    for (i = 0; i < NODES; i++) {
        if (nodes[i].used != 0 && &nodes[i] != n && nodes[i].parent == (unsigned)(n - nodes)) {
            return -ENOTEMPTY;
        }
    }
    free(n->data);
    n->data = NULL;
    n->size = n->capacity = 0;
    n->used = 0;
    return 0;
}

static int write_(struct node *n, const struct cf_req *r)
{
    unsigned end = r->off + r->len;

    if (end > n->capacity) {
        unsigned capacity = n->capacity == 0 ? 64 : n->capacity;
        char *data;

        while (capacity < end) {
            capacity *= 2;
        }
        data = realloc(n->data, capacity);
        if (data == NULL) {
            return -ENOSPC;
        }
        n->data = data;
        n->capacity = capacity;
    }
    if (r->off > n->size) {
        memset(n->data + n->size, 0, r->off - n->size);
    }
    memcpy(n->data + r->off, r->buf, r->len);
    if (end > n->size) {
        n->size = end;
    }
    return (int)r->len;
}

static int readdir_(const struct node *dir, struct cf_req *r)
{
    struct cf_dirent *de = r->buf;
    unsigned i, index = r->off;

    for (i = 0; i < NODES; i++) {
        if (nodes[i].used != 0 && &nodes[i] != dir && nodes[i].parent == (unsigned)(dir - nodes) &&
            index-- == 0) {
            strcpy(de->name, nodes[i].name);
            de->type = nodes[i].type;
            de->size = nodes[i].size;
            return 1;
        }
    }
    return 0;
}

static int serve(struct cf_req *r)
{
    struct node *n = node(r->node), *c;
    unsigned len;

    if (n == NULL) {
        return -ENOENT;                 /* removed since it was opened */
    }
    switch (r->op) {
    case CF_OP_LOOKUP:
        c = child(n, r->buf);
        if (c == NULL) {
            return -ENOENT;
        }
        r->new_node = number(c);
        r->type = c->type;
        r->size = c->size;
        return 0;
    case CF_OP_READ:
        if (r->off >= n->size) {
            return 0;
        }
        len = r->len < n->size - r->off ? r->len : n->size - r->off;
        memcpy(r->buf, n->data + r->off, len);
        return (int)len;
    case CF_OP_WRITE:
        len = (unsigned)write_(n, r);
        r->size = n->size;
        return (int)len;
    case CF_OP_READDIR:
        return readdir_(n, r);
    case CF_OP_CREATE:
        return create(n, r->buf, (int)r->off, r);
    case CF_OP_UNLINK:
        return unlink_(n, r->buf);
    case CF_OP_TRUNCATE:
        n->size = 0;
        r->size = 0;
        return 0;
    default:
        return -ENOTTY;
    }
}

int main(int argc, char **argv)
{
    struct cf_req *r;
    int port, rc;

    if (argc != 2) {
        fprintf(stderr, "usage: memfs directory &\n");
        return 2;
    }
    port = srv_create(0, 0);
    nodes[ROOT].used = 1;
    nodes[ROOT].type = CF_V_DIR;
    nodes[ROOT].parent = ROOT;
    rc = port < 0 ? port : srv_mount(port, argv[1], number(&nodes[ROOT]));
    if (rc < 0) {
        fprintf(stderr, "memfs: cannot mount at %s: error %d\n", argv[1], -rc);
        return 1;
    }
    while (srv_recv(port, &r, -1) > 0) {
        srv_reply(port, r, serve(r));
    }
    return 0;
}
