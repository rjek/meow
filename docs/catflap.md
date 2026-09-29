# Catflap: an operating system for MEOW

A proposal.  Catflap gives a MEOW microcontroller threads, processes,
devices and a file system, in C with assembly where the machine demands
it.  This document says what the hardware allows, what the system looks
like as a result, and in what order to build it.  `os/` is the
implementation, stage by stage as section 12 lists, and where this
document and the code differ the code is what was learnt; `attic/os/`
is the 2007 attempt in assembler and this supersedes it.

## 1. What the machine dictates

Five facts about MEOW decide most of the design.

- **No memory management and no privilege modes.**  Every instruction can
  reach every address.  A process cannot be protected from another, or
  the kernel from either.  `fork` is impossible, since an address space
  cannot be copied and moved, and a "process" is a bookkeeping unit
  rather than a boundary.  Isolation is by convention and by the
  compiler, exactly as on a Cortex-M0 or an AVR.
- **Two register banks, swapped by interrupt.**  An interrupt swaps banks
  and starts the handler at address 32; the handler can read and write
  every register of the interrupted bank as `ar0` to `apc`, and `IRQRTN`
  resumes wherever `apc` points.  A context switch is therefore sixteen
  `MOV`s into a control block, sixteen out, and `IRQRTN`.  The interrupt
  bank's own registers persist, so it keeps its stack pointer between
  interrupts for free.
- **No trap instruction.**  There is nothing a program can execute to
  enter the kernel other than a branch.  Since there is no privilege to
  gain, a system call is a function call; the only question is how a
  separately linked program finds the kernel, and the answer is a jump
  table at a fixed address in ROM.
- **Devices are discovered, not assumed.**  The Chairman's chip-select
  table names every device and its size.  Interrupts are one pending
  word and a mask; the timer is source 31; the console has no interrupt
  and must be polled.
- **Small.**  A useful machine has 64 KB to 1 MB of RAM and a ROM.  The
  kernel must fit in tens of KB, a thread must cost a few hundred bytes
  plus its stack, and nothing may assume memory is plentiful.

Everything below follows from these.

## 2. Shape

```
  +-----------------------------------------------------------------+
  |  programs: init, sh, ls, cat, lua ...   (ELF loaded from romfs)  |
  +-----------------------------------------------------------------+
  |  the shared C library: one copy in ROM, static data per process  |
  +-----------------------------------------------------------------+
  |  system call jump table (ROM, fixed address)                     |
  +-----------------------------------------------------------------+
  |  VFS: vnodes, mounts, fds, pipes    |  process: image, heap,    |
  |  romfs  devfs  ramfs  hostfs        |  fds, threads, exit        |
  +-------------------------------------+---------------------------+
  |  threads: scheduler, sleep, semaphores, message queues, timers   |
  +-----------------------------------------------------------------+
  |  devices: console, timer, memory; interrupt dispatch             |
  +-----------------------------------------------------------------+
  |  boot, context switch, interrupt entry              (assembler)  |
  +-----------------------------------------------------------------+
```

One kernel image in ROM, one address space, preemptive threads, a
non-preemptible kernel, and processes as the unit of loading and of
resource ownership.  Programs are ordinary `nmcc` output that call one
copy of the C library in ROM, the same PDCLib and musl as today with
the dozen platform functions calling the kernel instead of `msim`.

## 3. Memory layout

```
  0x00000000  ROM   reset vector, interrupt vector at 32, kernel code,
                    the shared C library's code, read-only data, the
                    initial values of .data; then the library's
                    relocation list and the romfs, found by their
                    position after the image
  0x08000000  RAM   kernel .data and .bss, the library's data and bss
                    kernel heap: control blocks, stacks, buffers,
                    process images, heaps and library data copies
                    interrupt stack (4 KB) and the boot thread's (4 KB),
                    at the top
```

The kernel learns the RAM size from the Chairman as `crt0` does now.
One allocator serves everything, a first-fit list with a 16-byte header
carrying the owning process, so that a process's exit frees whatever it
leaked.  dlmalloc is 6 KB of code and wants `sbrk`; the kernel's own
allocator should be under 1 KB.  Programs get `malloc` from libc, which
takes its arena from the kernel in large pieces.

There is no protection, so stacks carry a guard word at the bottom that
the scheduler checks at every switch, and a thread that has overrun is
killed with a message rather than silently corrupting its neighbour.

## 4. Threads

The kernel's unit of execution.  A thread control block holds the
sixteen registers, the state, priority, the process it belongs to, what
it is waiting for, a wakeup time, and links for the run and wait queues:
about 96 bytes.  Stacks come from the heap, 1 KB by default.

- **Scheduler.**  Fixed priorities, eight levels, round robin within a
  level.  The timer ticks at 100 Hz (10000 instructions under `msim`);
  a tick ends the running thread's slice if another of its priority is
  ready.  Priority 0 is the idle thread, which polls the console and
  halts under `msim` when nothing else can run, so that an idle system
  costs nothing.
- **Context switch.**  Always from interrupt mode.  A voluntary switch
  (block, yield, exit) sets `switch_wanted` and writes 1 to the
  Chairman's timer current-value register, so the tick fires after the
  next instruction; the handler then does the whole switch in the
  interrupt bank: save `ar0` to `apc` into the current block, pick the
  next, load, `IRQRTN`.  One switch path, one place where registers are
  saved, and no thread ever runs with interrupts off.  A tick that finds
  the kernel entered only counts itself; the kernel does the tick's
  work (waking sleepers, polling the console, ending a slice) when the
  call returns, and if that work queued the returning thread behind
  another it makes the timer fire again at once rather than running on
  from inside the ready queue, which is the bug the first version had.
- **Blocking.**  A thread blocks on a semaphore, a message queue, a
  sleep, or a vnode (a read from the console with nothing there).  Each
  has a wait queue; a wakeup moves the thread to the run queue and
  requests a switch if it outranks the running thread.
- **Synchronisation.**  Counting semaphores and mutexes with priority
  inheritance in their simplest form (the holder runs at the waiter's
  priority until release).  Message queues in the POSIX mould, fixed
  message size, bounded depth, blocking or not, are the IPC: they are
  what a driver thread and a client want, and they are what `mq_open`
  and friends map onto in libc.  No condition variables, no
  reader-writer locks; if they are wanted later they sit on top.
- **Interrupts.**  Handlers run in the interrupt bank, short, on the
  interrupt stack, and communicate with threads only by signalling a
  semaphore or posting to a queue.  A driver's real work is in a thread.

## 5. Processes

A process owns a loaded image, a heap, a table of open files, a working
directory, a name, an exit status and one or more threads.  It is
created by `spawn(path, argv)`, never by `fork`; `exec` is `spawn`
followed by `exit`.

- **Image format.**  Programs are ELF as `mld` writes it, linked with a
  new `-r` option that keeps the `ABS32`, `ABS16` and `ABS8` relocations
  and the section table.  The loader allocates one block for text, data
  and bss, copies, zeroes, and applies the relocations, which with three
  types is a hundred lines.  Position-dependent code with load-time
  relocation beats fixed load addresses, which would make two programs
  at once impossible, and beats position-independent code, which `nmcc`
  does not produce.
- **The shared C library.**  A program does not carry its own libc: one
  copy of PDCLib and musl's maths lives in ROM and every process calls
  it, as RISC OS programs call the SharedCLibrary.  The library's
  static data (the streams, `errno`, the heap state, `strtok`'s pointer)
  is instantiated once per process.  Section 6a says how.
- **Resources.**  Files are reference-counted vnodes; the fd table is
  per process, 16 entries, inherited by `spawn` for 0, 1 and 2 only.
  Memory is tagged by owner as above.  Threads are the process's; when
  the last exits, or any calls `exit`, the process ends and the kernel
  closes its files, frees its memory and wakes whoever is waiting in
  `wait`.
- **The kernel is process 0**, with no image, whose threads are the idle
  thread and the drivers'.  `init` is process 1, loaded from
  `/bin/init` in romfs, and runs a shell on the console.

## 6. System calls

A system call is a call to the kernel's own function by its address in
ROM.  The kernel is linked first, as an ELF executable as well as the
image, and `mld -S catflap.elf` makes every global symbol of it an
absolute one when a program is linked: `vfs_write` in a program is the
kernel's `vfs_write`, no stub, no table, no number.  `os/include/catflap.h`
declares the calls a program may make and the constants and layouts
they share with `os/kernel/kernel.h`.  Being function calls, they run on
the caller's stack in the caller's thread and can block.  The table of
branch targets at a fixed address that this document first proposed is
what to add if a program must ever outlive the kernel it was linked
against; nothing needs it yet.  The kernel is not preemptible: a thread inside a
system call is not switched away from until it blocks or returns, and a
tick that arrives meanwhile only notes that a switch is wanted.  One
counter says whether the kernel is entered; no lock, no mask fiddling,
and console latency bounded by the longest system call, which is fine
for a machine like this.

The initial set: `process_spawn`, `process_exit`, `process_wait`,
`process_pid`, `process_sbrk`, `thread_sleep`, `thread_yield`,
`ticks_now`, `kernel_time`, and `vfs_open`, `vfs_close`, `vfs_read`,
`vfs_write`, `vfs_seek`, `vfs_readdir`, `vfs_stat`, `vfs_ioctl`.  Errors
return negative `errno` values; libc turns them into `-1` and `errno`.

## 6a. The shared C library

A library that runs for every process at once with one copy of its code
must reach a different copy of its data for each.  `nmcc` addresses a
static by its absolute address, so the library is compiled in a new
mode, `-zsb`, in which the address of a variable in data or bss is the
address as linked plus a displacement, read from one word in kernel
RAM, `__client_sb`, through the backend's scratch register:

```
        LDR     r0, =stdout             ; the address as linked
        LDR     ir, =__client_sb
        LDR     ir, [ir]                ; this process's displacement
        ADD     r0, ir
```

Four instructions instead of one where a static's address is formed,
and nothing else changes: no register is reserved, the prologue is the
same, the address is a constant the compiler hoists out of loops like
any other, and a thread preempted between the load and the add keeps
its registers.  Functions, constant data and anything declared `const`
are addressed as they always were, since they stay in ROM.  The
scheduler writes `__client_sb` whenever it switches to a thread of
another process; the kernel's own threads have a displacement of 0.

The library is linked into the kernel image after two marker objects,
with `mld -B` laying bss out backwards so that the library's data and
bss are one range, `__libc_data_start` to `__libc_data_end`.  At
`spawn` a process is given a copy of that range, and the words in the
copy that pointed into the range (`stdout` at its `FILE`, the `FILE` at
its buffer) are moved along by the same displacement, from a list
`mld -R` writes and the ROM carries after the image.  RISC OS calls the
same thing relocation offsets.  Programs reach the library as they
reach the kernel, by absolute address (section 6).

What it costs: three instructions on each static address formation and
a 19 KB copy of the library's data per process, which is the tables
that PDCLib keeps (`printf`'s, the locale's, the time zone's) and
worth shrinking.  What it saves: the 18 KB a trivial program otherwise
carries, and all of the library that Lua would, per process, in ROM
and in RAM.  The compiler change is thirty lines in `gen.c`; the
kernel's is a hundred in `process.c`.

## 7. Devices and drivers

At boot the kernel walks the chip-select table and binds a driver to
each device ID it knows: ROM and RAM become memory regions, the Chairman
gives the timer and console, an IOC when one is specified gives whatever
it gives.  A driver is a vnode with `read`, `write`, `ioctl` and an
optional thread, registered under `/dev`.

- **Console.**  No interrupt, so the tick polls the flags register and
  posts each byte to a 64-byte queue; `read` on `/dev/console` takes
  from the queue, blocking.  Output is direct, with a mutex.  Under
  `msim`, output goes to the Chairman's serial register like everywhere
  else; the `BNV` calls are for the toolchain's tests, not the OS.
- **Timer.**  Source 31, the tick.  Sleeping threads sit in a list
  ordered by wakeup time; the tick pops what is due.  `uptime` counts
  ticks; `time` is uptime plus whatever `settime` was told.
- **Interrupt dispatch.**  The vector at 32 reads the pending word, masks
  it with what is enabled, and calls the handler for each set bit from
  a 32-entry table, then clears those bits and returns.  Timer first.

## 8. The virtual file system

Vnodes, as in every Unix since 4.3BSD, because it is the smallest
interface that lets `open` not care what is behind it.

```
struct vnode_ops {
    int (*lookup)(struct vnode *dir, const char *name, struct vnode **out);
    int (*read)(struct vnode *, void *buf, unsigned len, unsigned off);
    int (*write)(struct vnode *, const void *buf, unsigned len, unsigned off);
    int (*readdir)(struct vnode *, unsigned index, struct dirent *out);
    int (*create)(struct vnode *dir, const char *name, int type, struct vnode **out);
    int (*unlink)(struct vnode *dir, const char *name);
    int (*ioctl)(struct vnode *, int req, void *arg);
    void (*release)(struct vnode *);
};
```

A vnode is 32 bytes: the ops, a type, a size, a reference count, the
mount it belongs to and a word for the file system's use.  Path walking
is one function in the VFS that calls `lookup` a component at a time
and crosses mount points from a table of eight.  An open file is a
vnode, an offset and flags; an fd is an index into the process's table
of those.

File systems, in the order they are needed:

- **romfs.**  Read-only, appended to the ROM image by a host tool
  (`mkromfs`, in C, under `os/tools/`) from a directory tree: a header,
  a sorted table of full paths with offsets and sizes, then the data.
  Lookup is a binary search; `read` is `memcpy` from ROM.  Holds `/bin`
  and `/etc`.  The kernel finds it after its own image, so programs can
  be linked against the kernel and packed without relinking it.
- **devfs.**  `/dev`: the driver table presented as a directory.
- **ramfs.**  `/tmp`: files in heap-allocated blocks, for a program's
  scratch.  Directories are lists.
- **pipes.**  A 256-byte ring with a reader's and a writer's semaphore,
  created by `pipe` and used through the ordinary `read` and `write`.
  The shell's `|`.
- **hostfs.**  Development only: `/host` reaches the host's file system
  through new `msim` `BNV` calls (open, read, write, close, readdir),
  so that a program under test can read a script or write results
  without building a ROM.  Not part of the machine; a real MEOW would
  have an IOC with storage and a file system on it later.

`stdin`, `stdout` and `stderr` are fds 0, 1 and 2 on `/dev/console`
unless the shell redirected them.  libc's `fopen` maps onto `open`,
`fread` onto `read`, and the platform layer shrinks to those calls.

## 9. Userland

- **libc/catflap/**: a second platform layer beside `libc/meow/`, same
  PDCLib and musl, with `_PDCLIB_open` and the rest calling the kernel,
  `malloc` on `sbrk`, `setjmp` shared, and the POSIX-flavoured extras
  (`spawn`, `wait`, `mq_*`, `sem_*`, `thread_*`) in a `<catflap.h>`.
  One `libc.a` per platform.
- **init**: mounts `/dev` and `/tmp`, opens the console, spawns the
  shell, respawns it if it dies.
- **sh**: a small shell, commands with arguments, `|`, `<` and `>`,
  `&`, `cd`, `exit`, and nothing else.
- **utilities**: `ls`, `cat`, `echo`, `ps`, `free`, `mount`, `uptime`,
  `kill`.  Each a page of C.
- **lua**: the interpreter as built now, relinked against the Catflap
  libc, becomes the scripting language of the system and a test of it,
  and `io.open` starts to work.

## 10. What is in assembler

As little as possible, and all in one file, `boot.s`:

- the reset vector: set the interrupt bank's `sp`, copy `.data`, clear
  `.bss`, set up the Chairman, jump to `kmain`;
- the interrupt vector at 32: save nothing (the bank swap did), call the
  C dispatcher, and the switch path: the sixteen `MOV`s each way and
  `IRQRTN`;
- the system call table at 64, `DCD` per entry;
- `IRQRTN`, mask writes and the other things C cannot express, as short
  functions.

Everything else, drivers included, is C99 compiled by `nmcc`.  The
kernel does not use libc; it has its own `kprintf`, `memcpy` and string
functions in a few hundred lines, so that its size is its own and its
behaviour under interrupt is known.

## 11. Budget

| | Target |
|---|---|
| Kernel code | 24 KB, 32 KB with hostfs and ramfs |
| Kernel data and bss | 4 KB |
| Thread control block | 96 bytes |
| Default thread stack | 1 KB; interrupt stack 2 KB |
| Process overhead | 128 bytes plus image plus heap |
| Context switch | about 60 instructions |
| System call | 8 instructions on top of the function |
| Smallest useful machine | 64 KB RAM: kernel, init, shell, one utility |
| Comfortable | 256 KB: the above plus Lua |

## 12. Order of work

Each stage runs under `msim` with a test in `tests/os/` before the next
begins; the test harness is the existing one, standard input in and
output compared.

1. **Boot and threads.**  Done: `boot.s`, the allocator, the scheduler,
   the tick, `kprintf`.  `tests/os/threads.c`.
2. **Synchronisation and IPC.**  Done: semaphores, mutexes, message
   queues, the console polled by the tick.  `tests/os/sync.c`.
3. **VFS and romfs.**  Done: `mkromfs`, the vnode layer, devfs.
   `tests/os/vfs.c`.
4. **Processes and the shared library.**  Done: `mld -f cfx`, `-S`,
   `-B`, `-R`; the loader; `spawn`, `wait`, `exit`; `nmcc -zsb`; the
   library in the image with `libc/catflap/`.  `tests/os/proc.c` and
   `prog.c`: two processes use `printf`, `strtod`, `fopen`, `malloc`,
   `setjmp` and `atexit` at once from one copy of the library.
5. **Shell, pipes, ramfs, utilities.**  Done: pipes, `/tmp` in RAM,
   `dup2`, a working directory, `sh` with pipelines and redirection,
   `ls`, `cat`, `echo`, `wc`, `ps`, `free`, `uptime`, `sleep`.
   `tests/os/fs.c` and `shell.c`, a scripted session.  Two lessons: a
   program's own code is not displaced, so `stdin` and friends must be
   reached through a library function, not by address; and read-only
   data belongs in ROM, which `mld` now does, halving what a process
   costs on a 256 KB machine.
6. **hostfs and Lua.**  Test: Lua runs a script from `/host`.

## 13. Decisions taken, and open ones

Taken, for the reasons above: no `fork`; one address space and one
allocator; one shared C library with data per process; system calls as
function calls through a ROM table; every context switch from the
interrupt bank; a kernel that is never preempted; message queues rather
than signals; romfs before any writable file system; C everywhere but
`boot.s`.

Also taken: the instruction set does not change for the operating
system.  A trap operand that swapped banks would give a real system
call entry, but the design does not need one, and whether it is worth
having is a question to ask once the system runs and can be measured.

Multiprocessor: the Chairman was designed with masks for 32 CPUs so
that cores could bit-bang peripherals in the XMOS manner, and nothing
here supports that yet.  What the kernel does to avoid making it hard
later: the per-CPU state, which is the running thread, the kernel-entry
counter and `__client_sb`, sits in one structure that is indexed by
`BNV #2` when there is more than one of them, and kernel-wide state is
touched only by code that could take a lock.  The scheduler's queues
are shared, which is the right first shape for a few cores.
