/* procfs: the kernel's state as text, mounted at /proc.  Files are made
   afresh on every read, so a reader sees them as they are.  The root
   holds meminfo, uptime, mounts, version, self, and a directory for each
   process, named by its pid, holding status, cmdline and threads. */
#include "kernel.h"

#define TEXT_MAX 1024

enum { P_ROOT, P_PID, P_MEMINFO, P_UPTIME, P_MOUNTS, P_VERSION, P_CPUS,
       P_STATUS, P_CMDLINE, P_THREADS };

static const char *const root_files[] = { "meminfo", "uptime", "mounts", "version", "cpus" };
static const char *const pid_files[] = { "status", "cmdline", "threads" };

static const struct vnode_ops procdir_ops, proctext_ops;

/* The vnode's ino packs the kind and the pid */
static struct vnode *pnode(int kind, int pid)
{
    int dir = kind == P_ROOT || kind == P_PID;

    return vnode_new(dir ? &procdir_ops : &proctext_ops, dir ? V_DIR : V_FILE,
                     NULL, (uint32_t)kind | ((uint32_t)pid << 8), 0);
}

static int pkind(struct vnode *v)
{
    return (int)(v->ino & 0xff);
}

static int ppid(struct vnode *v)
{
    return (int)(v->ino >> 8);
}

static const char *state_name(int s)
{
    switch (s) {
    case T_READY: return "ready";
    case T_RUNNING: return "running";
    case T_SLEEPING: return "sleeping";
    case T_BLOCKED: return "blocked";
    default: return "done";
    }
}

/* The text of a file, into buf; its length, or an error */
static int generate(int kind, int pid, char *buf, size_t size)
{
    struct process *p = NULL;
    size_t n = 0;
    int i;

    if (kind >= P_STATUS) {
        p = process_find(pid);
        if (p == NULL) {
            return -ENOENT;
        }
    }
#define ADD(...) (n += (size_t)ksnprintf(buf + (n < size ? n : size), n < size ? size - n : 0, __VA_ARGS__))
    switch (kind) {
    case P_MEMINFO:
        ADD("total %u\nfree %u\n", (unsigned)CH_CS_SIZE(1), (unsigned)kmem_free());
        break;
    case P_UPTIME:
        ADD("%u.%02u\n", ticks_now() / HZ, ticks_now() % HZ);
        break;
    case P_MOUNTS:
        {
            const char *path, *type;

            for (i = 0; vfs_mount_info(i, &path, &type) > 0; i++) {
                ADD("%s %s\n", path, type);
            }
        }
        break;
    case P_VERSION:
        ADD("Catflap %s MEOW %s\n", CATFLAP_VERSION, cpu_model() == 0 ? "msim" : "MEOW1");
        break;
    case P_CPUS:
        for (i = 0; i < cpu_count(); i++) {
            struct cpu *c = cpu_of(i);

            if (c->online != 0) {
                ADD("%d %s%s\n", i, c->current->name,
                    c->exclusive != NULL ? " exclusive" : "");
            }
        }
        break;
    case P_STATUS:
        {
            size_t heap = 0;
            struct heapblk *b;

            for (b = p->heap; b != NULL; b = b->next) {
                heap += b->size;
            }
            ADD("name %s\npid %d\nparent %d\nthreads %d\nstate %s\ncwd %s\ncode %u %s\ndata %u\nheap %u\n",
                p->name != NULL ? p->name : "?", p->pid, p->parent != NULL ? p->parent->pid : -1,
                p->nthreads, p->dead != 0 ? "done" : "running", p->cwd,
                (unsigned)p->code_size, p->image == NULL ? "rom" : "ram",
                (unsigned)p->data_size, (unsigned)heap);
        }
        break;
    case P_CMDLINE:
        for (i = 0; p->argv != NULL && i < p->argc; i++) {
            ADD("%s%s", i > 0 ? " " : "", p->argv[i]);
        }
        ADD("\n");
        break;
    case P_THREADS:
        {
            struct thread *t;

            for (t = thread_list(); t != NULL; t = t->all) {
                if (t->proc == p) {
                    ADD("%d %s %d %u cpu%d\n", t->tid, state_name(t->state), t->prio,
                        (unsigned)t->stack_size, t->cpu);
                }
            }
        }
        break;
    default:
        return -EINVAL;
    }
#undef ADD
    return (int)(n < size ? n : size - 1);
}

static int parse_pid(const char *s)
{
    int v = 0;

    if (*s == '\0') {
        return -1;
    }
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9' || v > 100000) {
            return -1;
        }
        v = v * 10 + (*s - '0');
    }
    return v;
}

static int procdir_lookup(struct vnode *dir, const char *name, struct vnode **out)
{
    unsigned i;
    int pid;

    if (pkind(dir) == P_PID) {
        for (i = 0; i < sizeof pid_files / sizeof pid_files[0]; i++) {
            if (strcmp(name, pid_files[i]) == 0) {
                *out = pnode(P_STATUS + (int)i, ppid(dir));
                return *out == NULL ? -ENOMEM : 0;
            }
        }
        return -ENOENT;
    }
    for (i = 0; i < sizeof root_files / sizeof root_files[0]; i++) {
        if (strcmp(name, root_files[i]) == 0) {
            *out = pnode(P_MEMINFO + (int)i, 0);
            return *out == NULL ? -ENOMEM : 0;
        }
    }
    pid = strcmp(name, "self") == 0 ? current_process()->pid : parse_pid(name);
    if (pid < 0 || process_find(pid) == NULL) {
        return -ENOENT;
    }
    *out = pnode(P_PID, pid);
    return *out == NULL ? -ENOMEM : 0;
}

static int procdir_readdir(struct vnode *v, uint32_t index, struct dirent *de)
{
    const unsigned nroot = sizeof root_files / sizeof root_files[0];
    struct process *p;

    de->size = 0;
    if (pkind(v) == P_PID) {
        if (index >= sizeof pid_files / sizeof pid_files[0]) {
            return 0;
        }
        strcpy(de->name, pid_files[index]);
        de->type = V_FILE;
        return 1;
    }
    if (index < nroot) {
        strcpy(de->name, root_files[index]);
        de->type = V_FILE;
        return 1;
    }
    if (index == nroot) {
        strcpy(de->name, "self");
        de->type = V_DIR;
        return 1;
    }
    index -= nroot + 1;
    for (p = process_list(); p != NULL && index > 0; p = p->next) {
        index--;
    }
    if (p == NULL) {
        return 0;
    }
    ksnprintf(de->name, sizeof de->name, "%d", p->pid);
    de->type = V_DIR;
    return 1;
}

static int proctext_read(struct vnode *v, void *buf, size_t len, uint32_t off)
{
    char *text = kmalloc(TEXT_MAX);
    int n;

    if (text == NULL) {
        return -ENOMEM;
    }
    n = generate(pkind(v), ppid(v), text, TEXT_MAX);
    if (n >= 0) {
        if (off >= (uint32_t)n) {
            n = 0;
        } else {
            if (len > (uint32_t)n - off) {
                len = (uint32_t)n - off;
            }
            memcpy(buf, text + off, len);
            n = (int)len;
        }
    }
    kfree(text);
    return n;
}

static const struct vnode_ops procdir_ops = {
    procdir_lookup, NULL, NULL, procdir_readdir, NULL, NULL, NULL, NULL, NULL
};

static const struct vnode_ops proctext_ops = {
    NULL, proctext_read, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

struct vnode *procfs_init(void)
{
    return pnode(P_ROOT, 0);
}
