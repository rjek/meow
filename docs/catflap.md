# Catflap: an operating system for MEOW

Catflap gives a MEOW microcontroller threads, processes, devices and a
file system, in C with assembly where the machine demands it.  This
document began as the proposal and says what the hardware allows, what
the system looks like as a result, and in what order it was built.
`os/` is the implementation, every stage of section 12 done; where the
building changed the design the text says what was built.  `attic/os/`
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
  separately linked program finds the kernel, and the answer is that
  it is linked against the kernel's symbols (section 6).
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
  |  programs: init, sh, ls, cat, lua ...   (run in place in romfs)  |
  |  servers: programs that are devices and file systems  (6b)       |
  +-----------------------------------------------------------------+
  |  the shared C library: one copy in ROM, static data per process  |
  +-----------------------------------------------------------------+
  |  system calls: the kernel's functions, called by address         |
  +-----------------------------------------------------------------+
  |  VFS: vnodes, mounts, fds, pipes    |  process: image, heap,    |
  |  romfs  devfs  hostfs  ports        |  fds, threads, exit, kill  |
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
leaked.  The C library's `malloc` is 1 KB of code and wants `sbrk`; the
kernel's own allocator should be under 1 KB.  Programs get `malloc`
from libc, which takes its arena from the kernel in large pieces.

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
  (block, yield, exit) sets `switch_wanted` and rings the CPU's own
  doorbell in the Chairman, so an interrupt arrives after the next
  instruction; the handler then does the whole switch in the interrupt
  bank: save `ar0` to `apc` into the current block, pick the next,
  load, `IRQRTN`.  One switch path, one place where registers are
  saved, and no thread ever runs with interrupts off.  A tick that finds
  the kernel entered only counts itself; the kernel does the tick's
  work (waking sleepers, polling the console, ending a slice) when the
  call returns, and if that work queued the returning thread behind
  another it rings the doorbell rather than running on from inside the
  ready queue, which is the bug the first version had.  (The first
  versions forced the interrupt by writing 1 to the timer, which
  restarted the timer's period at every switch, so the clock lost time
  whenever threads switched often; the doorbell leaves the timer
  alone.)
- **CPUs.**  Each CPU runs a scheduler of its own, with its queues,
  its idle thread and its current thread in its local memory at chip
  select 30, where the shared C library also finds `__client_sb`, so
  Catflap needs local memory, if only a few hundred bytes.  A thread
  belongs to one CPU for life, the one it was made for, which is its
  maker's unless `thread_spawn_on`, `thread_create_on` or
  `process_spawn_on` says.  One lock, Chairman lock 0, is held by a
  CPU exactly while its `in_kernel` is above zero, so the kernel's data
  is touched by one CPU at a time, as it was by one thread at a time
  before: `kenter` takes the lock on the step from 0 to 1, `kexit`
  drops it on the step back, and the interrupt handler, which switches
  between threads with different counts, leaves it as the new count
  says.  A thread made ready on another CPU's queue rings that CPU's
  doorbell, so that its handler looks at what it has; the clock, the
  sleepers and the console are CPU 0's tick's business, and every CPU
  keeps its own slices with its own timer.  A thread made with
  `CPU_EXCLUSIVE` has its CPU to itself: nothing else is put there, and
  the CPU's timer is stopped, so only its own kernel calls and the
  doorbell interrupt it.  A thread that must end while it is running on
  another CPU is marked and its CPU rung; it ends at its next kernel
  exit, or, if it was in user code, when its CPU next switches it out
  and its registers can be rewritten to a call that exits.  CPU 0
  starts the others at boot through the Chairman's control blocks,
  each on an idle stack of its own, and they wait in `WFI` for work.
- **Blocking.**  A thread blocks on a semaphore, a message queue, a
  sleep, or a vnode (a read from the console with nothing there).  Each
  has a wait queue; a wakeup moves the thread to the run queue and
  requests a switch if it outranks the running thread.  A wait may have
  a time limit: the thread is then on a second list as well, which the
  tick walks.
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

- **Image format.**  Programs are `cfx` images, which `mld -f cfx`
  writes (`docs/linker.md` has the layout): a header, the code, the
  initialised data, and lists of the words that hold addresses.  The
  code reaches its data through the process's static base (section 6a),
  so only the data is per process.  A program in romfs is prelinked for
  where it sits and runs there; one from any other file system has its
  code copied to RAM and the listed words moved (section 12, stage 8).
  Position-dependent code with load-time relocation beats fixed load
  addresses, which would make two programs at once impossible, and
  beats position-independent code, which `nmcc` does not produce.
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
- **Kill.**  `process_kill(pid)` ends another process, which exits with
  status 137.  One of its threads is sent to `process_exit` in place of
  whatever it was doing and the rest end at once, so none of its code
  runs again.  There are no signals and nothing to catch: a machine
  with no protection cannot make a process's death its own business.
  Neither the kernel nor init can be killed.
- **The kernel is process 0**, with no image, whose threads are the idle
  thread and the drivers'.  `init` is process 1, run from
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
against; nothing needs it yet.

The kernel is not preemptible: a thread inside a system call is not
switched away from until it blocks or returns, and a tick that arrives
meanwhile only notes that a switch is wanted.  One counter says whether
the kernel is entered; no lock, no mask fiddling, and console latency
bounded by the longest system call, which is fine for a machine like
this.

The initial set: `process_spawn`, `process_exit`, `process_wait`,
`process_pid`, `process_sbrk`, `thread_sleep`, `thread_yield`,
`ticks_now`, `kernel_time`, and `vfs_open`, `vfs_close`, `vfs_read`,
`vfs_write`, `vfs_seek`, `vfs_readdir`, `vfs_stat`, `vfs_ioctl`.  Since
then: pipes, `dup`, directories, threads and named IPC, `vfs_poll`,
`process_kill`, and the `srv_` calls of section 6b.  Errors return
negative `errno` values; libc turns them into `-1` and `errno`.

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
a 5.4 KB copy of the library's data per process.  That was 19 KB at
first; keeping read-only data in ROM and dropping the time zone code
took the rest.  What it saves: the 18 KB a trivial program otherwise
carries, and all of the library that Lua would, per process, in ROM
and in RAM.  The compiler change is thirty lines in `gen.c`; the
kernel's is a hundred in `process.c`.

## 6b. Services: devices and file systems as programs

A program can be a device, or a file system, for every other program.
The kernel stays as it is, a driver or a file system is written, run,
killed and replaced like anything else in `/bin`, and what it costs is
RAM rather than ROM.  `/bin/memfs` is a file system of 200 lines:

```
$ memfs /mnt &
$ echo kept by a program > /mnt/note
$ cat /mnt/note
kept by a program
$ mount
...
user on /mnt
$ kill 3
$ ls /mnt
ls: /mnt: error 2
```

**Ports.**  A server makes a port with `srv_create`, which is a
descriptor, names what the port serves with `srv_dev(port, name, node)`
for `/dev/name` or `srv_mount(port, path, node)` for a file system, and
then loops: `srv_recv` for the next request, the work, `srv_reply` with
the result.  `node` is the server's own number for a file, anything it
likes; the kernel hands it back in every request about that file and
otherwise never looks at it.  Inside the kernel one set of `vnode_ops`
(`srv.c`) stands for every served file: each operation fills in a
request, queues it on the port, blocks the caller, and returns what the
server replied.  A request is the vnode operation and its arguments,
one for one (`os/include/catflap.h` has the table): lookup, read,
write, readdir, create, unlink, ioctl, truncate, and release if the
server asks to be told.

**Nothing is copied.**  There is one address space, so the request
record sits on the caller's stack and the server is given a pointer to
it, and the buffer in it is the caller's own, which the server reads or
writes where it lies.  What a request costs is its round trip: two
context switches, 340 instructions each as the kernel stands, and the
queueing and the two system calls at the server's end, 1,500
instructions in all against 170 for the same read from the kernel's
own `ramfs`.  That is two Lua function calls.  A server costs a
process: 5.5 KB of library data, a stack, and its heap if it uses one.

Those pointers are good for exactly as long as the caller is blocked,
and the rest of the design is about keeping it blocked until the server
has let go:

- **A server may keep a request**, taking others meanwhile, which is
  how a read waits for data to arrive: there is no other mechanism for
  blocking I/O, and no limit on how long.
- **Cancellation.**  If a caller is to die while a server holds its
  request, by `process_kill` or by another thread of its program
  exiting, the request is delivered to the server again with
  `cancelled` set.  The server must answer it at once, with anything.
  Until it does the caller stays, marked to end as it leaves the
  kernel; its call returns `EINTR` inside the kernel, so whatever the
  call held is released in the ordinary way.  A request the server had
  not yet taken is simply withdrawn.
- **The time limit.**  `srv_create` takes a number of ticks, or 0 for
  none.  A request fails with `ETIMEDOUT` if it waits that long while
  the server neither takes it nor, having taken it, comes back to
  `srv_recv` or to polling the port: a server that returns to its loop
  is alive, however long it keeps a request, and one that does not is
  hung.  A hung server still has the pointers, so it is not merely
  given up on: its port is closed and it is killed.  The limit is the
  server's promise about its own loop, which is why the server sets it.
- **The port closing**, because the server closed it, exited or was
  killed, fails every waiting request with `EIO` and removes its
  devices and mounts.  Files already open on it answer `EIO` from then
  on; the port's memory goes with the last of them.
- A server that uses a file it serves would wait for itself, and gets
  `EDEADLK` instead.

**Readiness.**  `vfs_poll(fds, n, ticks)` says which of some
descriptors can be read or written without waiting, and waits up to
`ticks` for the first.  Every kind of file answers through one more
vnode operation: pipes, the console, message queues, semaphores, ports
(readable when a request waits, so a server can watch its port along
with anything else) and served files, for which the server states the
answer with `srv_ready(port, node, mask)` and the kernel remembers it,
so that polling a served file costs no round trip.  There is one wait
queue for everyone who is polling, and anything that changes any
file's answer wakes them all to look again.  With a dozen threads that
is cheaper than a list of waiters on every file, and it is what an IP
stack would need: the kernel, not the application, decides who wakes.

**What is not there.**  No protection: a server can still scribble on
anything, and the structure is a microkernel's without the walls.  No
priority inheritance: a server runs at its own priority whoever is
waiting.  Path lookup is a request for each component.  Interrupts are
not delivered to programs yet, since the only interrupt is the timer's;
when there is hardware to drive, an interrupt will be one more kind of
request on a port.  `ramfs` was kept in the kernel at first, 1 KB of
ROM against the 10 KB of RAM a server costs, and then made a server
after all: `/etc/rc` starts `memfs /tmp` at boot, and a machine that
cannot spare the RAM leaves that line out and has no `/tmp`.

The other way to do this was to let a program register its own
`vnode_ops` and have the kernel call them on the caller's thread, with
`__client_sb` switched for the duration.  That is forty lines instead
of six hundred, and ten instructions a call instead of fifteen hundred.
It was not chosen because the server's code would run on a client's
stack, of a size the client chose; at any moment relative to the
server's own threads, in a library with no locks; and with nothing to
be done when the server exits but leave the kernel holding pointers
into freed memory.  A server that is a loop in an ordinary program is
easier to write and to get right.

## 7. Devices and drivers

At boot the kernel walks the chip-select table and binds a driver to
each device ID it knows: ROM and RAM become memory regions, the Chairman
gives the timer and console, an IOC when one is specified gives whatever
it gives.  A driver is a vnode with `read`, `write`, `ioctl` and an
optional thread, registered under `/dev`.  That is a driver in the
kernel; one that is a program registers through a port (section 6b).

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
- **`/tmp`** is `/bin/memfs`, a file system server (section 6b) that
  `init` starts from `/etc/rc`.  The kernel's own `ramfs` did the job
  until section 6b existed; it went, 1.5 KB of ROM, so that the ROM
  holds nothing a program can provide.
- **pipes.**  A 256-byte ring with a reader's and a writer's semaphore,
  created by `pipe` and used through the ordinary `read` and `write`.
  The shell's `|`.
- **hostfs.**  Development only: `/host` reaches the host's file system
  through new `msim` `BNV` calls (open, read, write, close, readdir),
  so that a program under test can read a script or write results
  without building a ROM.  Not part of the machine; a real MEOW would
  have an IOC with storage and a file system on it later.
- **ipcfs and procfs.**  `/ipc` holds the named message queues and
  semaphores, `/proc` the kernel's state as text (section 12, stage 7).
- **Served file systems.**  Any path a program has mounted itself on
  through a port (section 6b); `/proc/mounts` calls them `user`.

`stdin`, `stdout` and `stderr` are fds 0, 1 and 2 on `/dev/console`
unless the shell redirected them.  libc's `fopen` maps onto `open`,
`fread` onto `read`, and the platform layer shrinks to those calls.

## 9. Userland

- **libc/catflap/**: a second platform layer beside `libc/meow/`, same
  PDCLib and musl, with `_PDCLIB_open` and the rest calling the kernel,
  `malloc` on `sbrk`, `setjmp` shared, and the POSIX-flavoured extras
  (`spawn`, `wait`, `mq_*`, `sem_*`, `thread_*`) in a `<catflap.h>`.
  One `libc.a` per platform.
- **init**: spawns the shell on the console, starts another if it
  fails, and ends, halting the machine, when it exits cleanly.  The
  kernel has mounted everything by then.
- **sh**: a small shell, commands with arguments and quoting, `|`, `<`
  and `>`, `&`, `cd`, `exit`, and nothing else.
- **utilities**: `ls`, `cat`, `echo`, `wc`, `mkdir`, `rm`, `ps`, `free`,
  `mount`, `uname`, `uptime`, `sleep`, `kill`.  Each a page of C, and
  120 to 800 bytes in ROM.
- **memfs**: a file system in a program's memory, mounted where its
  argument says, and the model for writing another: 1.6 KB.
- **lua**: the stock interpreter, linked against the shared library, is
  the scripting language of the system and a test of it.  It is 178 KB
  in romfs and runs there.

## 10. What is in assembler

As little as possible, and all in one file, `boot.s`:

- the reset vector: set the interrupt bank's `sp`, copy `.data`, clear
  `.bss`, set up the Chairman, jump to `kmain`;
- the interrupt vector at 32: save nothing (the bank swap did), call the
  C dispatcher, and the switch path: the sixteen `MOV`s each way and
  `IRQRTN`;
- the space at 64 kept for a system call table, empty (section 6);
- `IRQRTN`, mask writes and the other things C cannot express, as short
  functions.

Everything else, drivers included, is C99 compiled by `nmcc`.  The
kernel does not use libc; it has its own `kprintf`, `memcpy` and string
functions in a few hundred lines, so that its size is its own and its
behaviour under interrupt is known.

## 11. Budget

The targets the proposal set.  As built the kernel is 27 KB of code
with every file system in it, 5.5 KB of that being ports, poll and
kill, and the ROM 360 KB, of which the C library and the arithmetic
runtime are 133 KB and Lua 178 KB; `docs/rom-size.md` has the
breakdown.

| | Target |
|---|---|
| Kernel code | 24 KB, 30 KB with hostfs |
| Kernel data and bss | 4 KB |
| Thread control block | 96 bytes |
| Default thread stack | 1 KB; a program's main thread 4 KB; interrupt stack 4 KB |
| Process overhead | 128 bytes plus data plus heap |
| Context switch | about 60 instructions; as built, 340 |
| System call | a function call |
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
6. **hostfs and Lua.**  Done: `msim -H` lends a directory through
   `BNV #-18` and the kernel mounts it at `/host`; the stock Lua 5.4.7
   is a Catflap program, `/bin/lua`, calling the shared C library, so
   `io.open` works on every file system.  To get there: a process heap
   of chained blocks (`sbrk` is not contiguous, which the C library's
   allocator copes with), a stack size in the `cfx` header (`mld -k`;
   Lua asks for 16 KB), a guard word at the foot of every thread stack
   checked at each switch, orphans handed to the kernel and freed when
   they end,
   `process_waitany` so the shell reaps background jobs, quoting in the
   shell, and init halting the machine when the shell ends cleanly.
   `tests/os/lua.c`, with `tests/os/lua.host` as `/host`.  `make -C os
   run` boots with 1 MB and `os/` as `/host`.
7. **Userland IPC, `/proc` and tools.**  Done.  Message queues and
   semaphores are files in `/ipc`: `ipc_create` names one, `vfs_open`
   opens it, a queue's `write` sends one message and its `read` takes
   one, a semaphore's `read` waits and its `write` posts, `vfs_ioctl`
   has the versions that do not block, and a child inherits them like
   any descriptor.  An object lives while it is named or open.  A
   program starts threads with `thread_spawn`; the C library keeps no
   locks, so only one thread at a time may use stdio or `malloc`.
   `/proc` holds `meminfo`, `uptime`, `mounts`, `version`, `self` and a
   directory per process with `status`, `cmdline` and `threads`,
   generated at each read; `ps`, `free`, `mount` and `uname` read it.
   `mkdir` and `rm` join the tools.  `tests/os/ipc.c` runs
   `/bin/ipctest`: three threads on a work queue, a second process
   answering over named queues after a semaphore says it is ready.
8. **Execute in place.**  Done.  Programs are compiled with `-zsb`, as
   the library is, and their data is linked at `__user_data_base`, just
   past the library's data range, so one static base per process serves
   both: a process's data block is the library's data followed by the
   program's.  `mkromfs -b` knows where the romfs will sit in ROM and
   prelinks each program for where it lands.  At `spawn` the kernel asks
   the file system for the file's address (`VFS_IOC_ADDR`, which romfs
   answers); if the code is linked for exactly that address it runs
   there, and otherwise, from `/tmp`, `/host`, or romfs built without
   `-b`, it is copied to RAM and its code addresses moved, as before.
   Either way only the data is copied, and a process that runs Lua now
   costs 17 KB of data (6 KB since the library's shrank) and its heap
   rather than 280 KB, and starts in a quarter of the instructions;
   `-zsb` made no measurable difference to Lua's speed.  `tests/os/xip.c` runs one program from ROM, from
   `/host` and from a copy in `/tmp`.  Two rules follow.  Every object
   in a program must be compiled with `-zsb`, since a word in code
   holding a data address is never moved; mixing in one that is not
   corrupts the kernel.  And a `const` object holding the address of
   writable data, `int *const p = &x;`, sits in ROM with the linked
   address in it and so points at the wrong place; nothing in the
   library or the programs does this.
   in ROM to something also in ROM, as Lua's tables of names are, is
   fine, and the compiler knows not to displace its address.)
9. **Services, poll and kill.**  Done: section 6b.  `srv.c` is the
   ports; `vfs_poll` and a `poll` operation on every kind of file;
   waits with a time limit; `process_kill` and `/bin/kill`; `/bin/memfs`.
   `tests/os/srv.c` runs `/bin/srvtest`, which is a device server and
   its clients in one: reads the server keeps until there is data,
   polls with and without a limit, a client killed while the server
   holds its request, a program exiting under a thread that waits on
   the server, a server that exits with requests unanswered, one that
   stops answering and is killed for it, and `memfs` with a program
   copied into it and run from it.  The kernel ends with no memory
   lost.  One lesson: a thread told to die while a server holds its
   request cannot just end when the answer comes, because the call it
   was making holds references that only returning through the call
   lets go of; so the call fails and the thread ends on its way out of
   the kernel.
10. **A smaller ROM.**  Lua is an option, `make WITH_LUA=1`; the test
    programs are built for the tests but not put in the ROM; and
    `<math.h>` is `os/obj/libm.a`, linked into the programs that call
    it, since nothing in the shell or the tools does.  A binary built
    later links the same archive.  The ROM is 104 KB: 94 KB of kernel
    and C library, 10 KB of programs.
11. **Servers at boot.**  `init` runs `/etc/rc` before the shell: one
    command a line, `&` to leave one running, and `mounted PATH` to
    wait for a server to have mounted somewhere.  The default `rc`
    starts `memfs /tmp`, and the kernel's `ramfs` is gone.  The tests
    that use `/tmp` start the server the same way.
12. **More than one CPU.**  Done: section 4's last point.  The per-CPU
    structure moved into local memory, with `__client_sb` an absolute
    symbol in it defined by `boot.s`; `kenter` and `kexit` take and
    drop the lock; the queues became per-CPU; `cpu_start` brings a CPU
    up and `cpu_main` is its idle loop; `thread_spawn_on`,
    `process_spawn_on`, `cpu_count` and `cpu_id` are the calls, and
    `/proc/cpus` and a `cpuN` column in `/proc/PID/threads` show the
    state.  `tests/os/cpus.c` runs with `msim -n 2 -j 5`: a thread on
    CPU 1 trading a semaphore and a mutex with init on CPU 0, a sleeper
    there, `/bin/hello` run there through the shared library, a
    program whose thread reports which CPU it ran on, a spinning
    program killed while it runs on CPU 1, and an exclusive thread,
    which refuses company and lets go when told.  The output is the
    same for every interleaving tried and with three and four CPUs.
    One change to the clock came with it: a forced switch rings the
    doorbell instead of restarting the timer, so ticks no longer slip
    under heavy switching, and two tests' expectations moved by a
    tick.  Workers' stacks are still in shared RAM; putting an
    exclusive thread's stack in its CPU's local memory, for timing that
    is a matter of counting instructions, is the next thing to do
    there.

## 13. Decisions taken, and open ones

Taken, for the reasons above: no `fork`; one address space and one
allocator; one shared C library with data per process; system calls as
function calls to the kernel's own addresses; every context switch
from the interrupt bank; a kernel that is never preempted; message
queues rather than signals; romfs before any writable file system; C
everywhere but `boot.s`; drivers and file systems as programs behind
ports, answering requests, rather than as code the kernel calls.

Also taken: the instruction set does not change for the operating
system.  A trap operand that swapped banks would give a real system
call entry, but the design does not need one, and whether it is worth
having is a question to ask once the system runs and can be measured.

Multiprocessor: the Chairman was designed with masks for 32 CPUs so
that cores could bit-bang peripherals in the XMOS manner; the rest of
the architecture is in `reference.md` section 5.5 and the kernel now
uses it, section 4 above and stage 12 below.  `multicore.md` has the
argument that led there.
