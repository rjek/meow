/* Processes: a loaded image, a copy of the shared library's data, a
   table of open files and some threads.  There is no fork: spawn loads
   a program from the file system and starts it. */
#include "kernel.h"

/* The shared library's data range and where the ROM keeps the list of
   words in it that point into it (see libc_start.s and mld -R). */
extern char __libc_data_start[], __libc_data_end[];
uint32_t __client_sb;                   /* the running process's displacement */
static const uint32_t *lib_relocs;
static uint32_t lib_nrelocs;

struct process kproc = { 0, "kernel" };
static struct process *procs = &kproc;
static int next_pid = 1;

struct process *current_process(void)
{
    return this_cpu()->current->proc;
}

struct process *process_find(int pid)
{
    struct process *p;

    for (p = procs; p != NULL; p = p->next) {
        if (p->pid == pid) {
            return p;
        }
    }
    return NULL;
}

void process_init(const uint32_t *relocs, uint32_t n)
{
    lib_relocs = relocs;
    lib_nrelocs = n;
    waitq_init(&kproc.waiters);
    waitq_init(&kproc.childq);
}

/* A private copy of the library's data for a new process, with the
   pointers in it moved along. */
static int lib_instance(struct process *p)
{
    size_t size = (size_t)(__libc_data_end - __libc_data_start) + 4;
    uint32_t i;

    p->libdata = kmalloc(size);
    if (p->libdata == NULL) {
        return -ENOMEM;
    }
    memcpy(p->libdata, __libc_data_start, size);
    p->sb = (uint32_t)((char *)p->libdata - __libc_data_start);
    for (i = 0; i < lib_nrelocs; i++) {
        uint32_t *word = (uint32_t *)((char *)lib_relocs[i] + p->sb);

        *word += p->sb;
    }
    return 0;
}

/* Read a whole file into memory it allocates. */
static int slurp(const char *path, char **out, uint32_t *size)
{
    struct stat st;
    int rc = vfs_stat(path, &st), fd, n;
    char *buf;
    uint32_t got = 0;

    if (rc < 0) {
        return rc;
    }
    if (st.type != V_FILE) {
        return -EISDIR;
    }
    fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        return fd;
    }
    buf = kmalloc(st.size + 1);
    if (buf == NULL) {
        vfs_close(fd);
        return -ENOMEM;
    }
    while (got < st.size && (n = vfs_read(fd, buf + got, st.size - got)) > 0) {
        got += (uint32_t)n;
    }
    vfs_close(fd);
    if (got != st.size) {
        kfree(buf);
        return -EIO;
    }
    *out = buf;
    *size = st.size;
    return 0;
}

struct cfx_header {
    char magic[4];
    uint32_t image_size, mem_size, entry, nrelocs;
    uint32_t stack;                     /* bytes the main thread wants, or 0 */
};

/* Load a cfx image: copy, zero the rest, add the load address to every
   word the linker listed. */
static int load_image(struct process *p, const char *path, uint32_t *entry)
{
    char *file;
    uint32_t size, i;
    const struct cfx_header *h;
    const uint32_t *relocs;
    int rc = slurp(path, &file, &size);

    if (rc < 0) {
        return rc;
    }
    h = (const struct cfx_header *)file;
    if (size < sizeof *h || memcmp(h->magic, "CFX1", 4) != 0 ||
        sizeof *h + h->image_size + 4 * h->nrelocs > size ||
        h->mem_size < h->image_size) {
        kfree(file);
        return -ENOEXEC;
    }
    p->image = kmalloc(h->mem_size);
    if (p->image == NULL) {
        kfree(file);
        return -ENOMEM;
    }
    p->image_size = h->mem_size;
    memcpy(p->image, file + sizeof *h, h->image_size);
    memset(p->image + h->image_size, 0, h->mem_size - h->image_size);
    relocs = (const uint32_t *)(file + sizeof *h + h->image_size);
    for (i = 0; i < h->nrelocs; i++) {
        if (relocs[i] + 4 > h->image_size) {
            kfree(file);
            return -ENOEXEC;
        }
        *(uint32_t *)(p->image + relocs[i]) += (uint32_t)p->image;
    }
    *entry = (uint32_t)p->image + h->entry;
    p->stack_size = h->stack != 0 ? (h->stack + 7) & ~7u : STACK_USER;
    kfree(file);
    return 0;
}

static void heap_free(struct process *p)
{
    while (p->heap != NULL) {
        struct heapblk *b = p->heap;

        p->heap = b->next;
        kfree(b);
    }
}

/* Everything a process owns but its threads, which have gone already. */
static void process_free(struct process *p)
{
    struct process **pp;

    for (pp = &procs; *pp != p; pp = &(*pp)->next) {
    }
    *pp = p->next;
    kfree(p->image);
    kfree(p->libdata);
    kfree(p->argv);
    heap_free(p);
    kfree(p);
}

/* Free the orphans that have ended: nobody else will. */
void reap_orphans(void)
{
    struct process *p = procs, *next;

    kenter();
    for (; p != NULL; p = next) {
        next = p->next;
        if (p->orphan != 0 && p->dead != 0 && p->nthreads == 0) {
            process_free(p);
        }
    }
    kexit();
}

/* argv, copied into the new process's memory as one block: the pointer
   table, then the strings. */
static char **copy_args(int argc, char *const argv[])
{
    size_t size = (size_t)(argc + 1) * sizeof(char *);
    char **table, *strings;
    int i;

    for (i = 0; i < argc; i++) {
        size += strlen(argv[i]) + 1;
    }
    table = kmalloc(size);
    if (table == NULL) {
        return NULL;
    }
    strings = (char *)(table + argc + 1);
    for (i = 0; i < argc; i++) {
        table[i] = strings;
        strcpy(strings, argv[i]);
        strings += strlen(argv[i]) + 1;
    }
    table[argc] = NULL;
    return table;
}

static int process_main(void *arg);

/* Start a program.  Returns its pid, or an error. */
int process_spawn(const char *path, int argc, char *const argv[])
{
    struct process *parent = current_process();
    struct process *p;
    uint32_t entry;
    int rc, i;

    kenter();
    p = kmalloc(sizeof *p);
    if (p == NULL) {
        kexit();
        return -ENOMEM;
    }
    memset(p, 0, sizeof *p);
    p->parent = parent;
    waitq_init(&p->waiters);
    waitq_init(&p->childq);
    rc = load_image(p, path, &entry);
    if (rc == 0) {
        rc = lib_instance(p);
    }
    if (rc == 0) {
        p->argv = copy_args(argc, argv);
        if (p->argv == NULL) {
            rc = -ENOMEM;
        }
    }
    if (rc < 0) {
        kfree(p->image);
        kfree(p->libdata);
        kfree(p);
        kexit();
        return rc;
    }
    p->pid = next_pid++;
    p->argc = argc;
    p->entry = entry;
    p->name = p->argv[0];
    strcpy(p->cwd, parent->cwd);
    for (i = 0; i < 3; i++) {           /* the standard streams come along */
        p->fds[i] = parent->fds[i];
        if (p->fds[i] != NULL) {
            p->fds[i]->refs++;
        }
    }

    p->next = procs;
    procs = p;
    p->main = thread_create_in(p, p->name, process_main, p, PRIO_DEFAULT, p->stack_size);
    if (p->main == NULL) {
        procs = p->next;
        kfree(p->image);
        kfree(p->libdata);
        kfree(p->argv);
        kfree(p);
        kexit();
        return -ENOMEM;
    }
    p->nthreads = 1;
    kexit();
    return p->pid;
}

/* The first thread of a process: enter the image as start(argc, argv),
   and should that return, exit with what it returned. */
static int process_main(void *arg)
{
    struct process *p = arg;
    int (*start)(int, char **) = (int (*)(int, char **))p->entry;

    process_exit(start(p->argc, p->argv));
    return 0;
}

/* End the current process: every other thread of it dies now, its files
   close, its children become the kernel's, and whoever waits for it is
   told.  The memory goes when it is waited for, or, for an orphan, as
   soon as it ends. */
void process_exit(int status)
{
    struct process *p = current_process(), *q;
    int i;

    kenter();
    if (p == &kproc) {
        kpanic("the kernel exits");
    }
    p->exit_status = status;
    p->dead = 1;
    for (q = procs; q != NULL; q = q->next) {
        if (q->parent == p) {
            q->parent = &kproc;
            q->orphan = 1;
        }
    }
    thread_kill_others(p);
    for (i = 0; i < NFD; i++) {
        if (p->fds[i] != NULL) {
            vfs_close(i);
        }
    }
    while (waitq_wake_one(&p->waiters) != NULL) {
    }
    thread_exit(status);
}

/* Wait for a child to end, and free it.  Returns 0 and its status. */
int process_wait(int pid, int *status)
{
    struct process *p;

    kenter();
    p = process_find(pid);
    if (p == NULL || p->parent != current_process() || p->orphan != 0) {
        kexit();
        return -ECHILD;
    }
    while (p->dead == 0 || p->nthreads != 0) {
        waitq_wait(&p->waiters);
    }
    reap_zombies();                     /* its threads, before its memory is counted */
    if (status != NULL) {
        *status = p->exit_status;
    }
    process_free(p);
    kexit();
    return 0;
}

/* Any child that has ended: its pid, 0 if none has and block is 0, or
   -ECHILD if there are no children at all. */
int process_waitany(int *status, int block)
{
    struct process *me = current_process(), *p;
    int pid;

    kenter();
    for (;;) {
        int children = 0;

        for (p = procs; p != NULL; p = p->next) {
            if (p->parent != me || p->orphan != 0) {
                continue;
            }
            children++;
            if (p->dead != 0 && p->nthreads == 0) {
                break;
            }
        }
        if (p != NULL) {
            break;
        }
        if (children == 0 || block == 0) {
            kexit();
            return children == 0 ? -ECHILD : 0;
        }
        waitq_wait(&me->childq);
    }
    reap_zombies();
    pid = p->pid;
    if (status != NULL) {
        *status = p->exit_status;
    }
    process_free(p);
    kexit();
    return pid;
}

int process_pid(void)
{
    return current_process()->pid;
}

struct process *process_list(void)
{
    return procs;
}

/* The program's heap: blocks taken from the kernel's as it grows, each
   handed out in pieces.  Blocks need not be next to each other, and the
   C library's allocator is built knowing that.  It never gives memory
   back, so a negative increment is refused. */
void *process_sbrk(int increment)
{
    struct process *p = current_process();
    struct heapblk *b = p->heap;
    char *old;

    kenter();
    if (increment < 0) {
        kexit();
        return (void *)-1;
    }
    if (b == NULL || (size_t)increment > b->size - b->used) {
        size_t size = (size_t)increment > HEAP_CHUNK ? (size_t)increment : HEAP_CHUNK;

        b = kmalloc(sizeof *b + size);
        if (b == NULL) {
            kexit();
            return (void *)-1;
        }
        b->size = size;
        b->used = 0;
        b->next = p->heap;
        p->heap = b;
    }
    old = (char *)(b + 1) + b->used;
    b->used += (size_t)increment;
    kexit();
    return old;
}

/* Called by the scheduler when a process's thread has gone for good. */
void process_thread_gone(struct process *p)
{
    p->nthreads--;
    if (p->nthreads == 0 && p->dead != 0) {
        while (waitq_wake_one(&p->waiters) != NULL) {
        }
        if (p->parent != NULL) {
            while (waitq_wake_one(&p->parent->childq) != NULL) {
            }
        }
    }
}
