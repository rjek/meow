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
    char magic[4];                      /* CFX2 */
    uint32_t code_size, data_size, bss_size;
    uint32_t entry;                     /* offset into the code */
    uint32_t stack;                     /* bytes the main thread wants, or 0 */
    uint32_t code_base;                 /* where the code's addresses assume it is */
    uint32_t data_base;                 /* where its data is linked: __user_data_base */
    uint32_t ncc, ndc, ndd;             /* code-in-code, code-in-data, data-in-data */
    uint32_t reserved;
};

extern char __user_data_base[];

/* Load a program.  Its code runs where it is when the file is in ROM at
   the address mkromfs linked it for; otherwise it is copied to RAM and
   its code addresses moved.  Either way the process gets one block
   holding the library's data and then the program's, with the pointers
   in both moved by the process's static base. */
static int load_image(struct process *p, const char *path, uint32_t *entry)
{
    const struct cfx_header *h;
    const char *file, *code, *data;
    const uint32_t *cc, *dc, *dd;
    char *buf = NULL, *block;
    uint32_t size, i, delta, lib_size, prefix;
    int rc, fd = vfs_open(path, O_RDONLY);
    struct stat st;

    if (fd < 0) {
        return fd;
    }
    file = NULL;
    vfs_ioctl(fd, VFS_IOC_ADDR, &file);
    vfs_close(fd);
    rc = vfs_stat(path, &st);
    if (rc < 0) {
        return rc;
    }
    size = st.size;
    if (file == NULL) {
        rc = slurp(path, &buf, &size);
        if (rc < 0) {
            return rc;
        }
        file = buf;
    }
    h = (const struct cfx_header *)file;
    if (size < sizeof *h || memcmp(h->magic, "CFX2", 4) != 0 ||
        h->code_size % 4 != 0 || h->data_size % 4 != 0 ||
        sizeof *h + h->code_size + h->data_size + 4 * (h->ncc + h->ndc + h->ndd) > size ||
        h->entry >= h->code_size || h->data_base != (uint32_t)__user_data_base) {
        kfree(buf);
        return -ENOEXEC;                /* not a program, or not linked for this kernel */
    }
    code = file + sizeof *h;
    data = code + h->code_size;
    cc = (const uint32_t *)(data + h->data_size);
    dc = cc + h->ncc;
    dd = dc + h->ndc;
    for (i = 0; i < h->ncc; i++) {
        if (cc[i] % 4 != 0 || cc[i] + 4 > h->code_size) {
            kfree(buf);
            return -ENOEXEC;
        }
    }
    for (i = 0; i < h->ndc + h->ndd; i++) {
        if (dc[i] % 4 != 0 || dc[i] + 4 > h->data_size) {
            kfree(buf);
            return -ENOEXEC;
        }
    }
    p->code_size = h->code_size;
    if (buf == NULL && h->code_base == (uint32_t)code) {
        p->image = NULL;                /* in place */
        p->code = code;
    } else {
        p->image = kmalloc(h->code_size);
        if (p->image == NULL) {
            kfree(buf);
            return -ENOMEM;
        }
        memcpy(p->image, code, h->code_size);
        delta = (uint32_t)p->image - h->code_base;
        for (i = 0; i < h->ncc; i++) {
            *(uint32_t *)(p->image + cc[i]) += delta;
        }
        p->code = p->image;
    }
    delta = (uint32_t)p->code - h->code_base;

    lib_size = (uint32_t)(__libc_data_end - __libc_data_start) + 4;
    prefix = (uint32_t)(__user_data_base - __libc_data_start);
    p->data_size = prefix + h->data_size + h->bss_size;
    block = kmalloc(p->data_size);
    if (block == NULL) {
        kfree(p->image);
        p->image = NULL;
        kfree(buf);
        return -ENOMEM;
    }
    memcpy(block, __libc_data_start, lib_size);
    memset(block + lib_size, 0, prefix - lib_size);
    memcpy(block + prefix, data, h->data_size);
    memset(block + prefix + h->data_size, 0, h->bss_size);
    p->libdata = block;
    p->sb = (uint32_t)(block - __libc_data_start);
    for (i = 0; i < lib_nrelocs; i++) {
        *(uint32_t *)((char *)lib_relocs[i] + p->sb) += p->sb;
    }
    for (i = 0; i < h->ndc; i++) {
        *(uint32_t *)(block + prefix + dc[i]) += delta;
    }
    for (i = 0; i < h->ndd; i++) {
        *(uint32_t *)(block + prefix + dd[i]) += p->sb;
    }
    *entry = (uint32_t)p->code + h->entry;
    p->stack_size = h->stack != 0 ? (h->stack + 7) & ~7u : STACK_USER;
    kfree(buf);
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
