# More than one core: a proposal

The Chairman was designed with interrupt masks for 32 CPUs and `BNV #2`
reports a CPU's bus ID, and that is all the specification says about
having more than one.  This began as a proposal for the rest, written
before any of it was implemented so that the questions could be argued
about first.  The architecture it proposes is now in `reference.md`,
sections 1.5, 1.7, 3, 4, 5.2, 5.3, 5.5 and 5.6, and `decisions.md` item
19 says why; the operating system's part is still to do.  Where the
two differ, the reference is right: the per-CPU timer went into the
control block, the locks moved to 0x2e00 and the present mask to
0x2c00.

## Keep it or drop it

Dropping it would free 128 bytes of Chairman address space and one BNV
operand, which is nothing.  Keeping it costs, in hardware, one more
copy of the core (1,500 to 2,500 LUT4s, see `hardware.md`) and a bus
arbiter, and in the specification perhaps forty lines.  The reason to
keep it is the reason it was designed in: a core that does nothing but
drive a few GPIO lines with instruction-exact timing is a peripheral
that needs no VHDL, and on an FPGA board with a hundred pins and a
tight supply of block RAM, cores are cheaper than peripherals.  That
is the XMOS argument and it holds here.

What is proposed is the XMOS shape and not the SMP one.  Threads do
not migrate.  Core 0 runs Catflap, every process and every server,
exactly as today.  The other cores are *workers*: each runs one
function, given to it by a process on core 0, with no scheduler, no
kernel entry and no interrupts it did not ask for, until it finishes
or is stopped.  A worker is a peripheral written in C.

## The architecture

Four additions, and one rule.  None of them changes an instruction
encoding.

### Starting a core: Chairman core control

At reset only CPU 0 runs; every other CPU is held in reset by the
Chairman until it is released, and so the ROM needs no parking code
and the boot does not have thirty-one cores fetching the same reset
vector.  A block of registers per CPU, at `0x2800 + 0x20 * cpu`:

| Offset | Access | Register |
|---|---|---|
| +0x00 | R | Status: bit 0 present, bit 1 running |
| +0x04 | RW | Start address |
| +0x08 | W | Control: writing 1 starts the CPU at the start address with every register zero and the normal bank active; writing 0 stops it and holds it in reset |
| +0x0c | W | Doorbell: writing any value raises interrupt source 30 for that CPU |

and `0x2a00`, read-only, a word with a bit set for every CPU present.

Writing 0 to control is how a worker that has wedged is recovered,
and it is what the kernel does to a process's workers when the
process dies.  A core starts with nothing in its registers but `pc`,
so the first thing it runs is a trampoline that reads `BNV #2` and
loads `sp`, `sb` and the function's argument from a table core 0
filled in before pressing the control register; that is a dozen
instructions in `boot.s`.

### Telling a core something: per-CPU pending and the doorbell

Today there is one pending register.  With several CPUs it becomes
one word per CPU, at `0x2200 + 4 * cpu`: a hardware source sets its
bit in every CPU's word, each CPU clears its own, and a source is
delivered to the CPUs whose masks have it.  `0x2400` stays as it is,
defined as an alias of the reading CPU's own word, so a one-CPU
system, msim today and the whole of Catflap today see no change.

The doorbell is the inter-processor interrupt: core 0 rings a worker
to tell it there is work, a worker rings core 0 to say it has
finished or filled a buffer.  It is source 30, the one source whose
pending bit is per-CPU from the start; the Chairman has 8 to 29
still free.

### Agreeing on something: Chairman locks

Thirty-two words at `0x2c00 + 4 * n`.  Reading one returns its value
and sets it to 1; writing one sets it to what is written.  A read
that returns 0 has taken the lock, and a write of 0 releases it.
This is test-and-set done by the one device that already sees every
bus access in order, which is why it costs thirty-two flip-flops and
no change to the bus protocol.

The alternative is an atomic swap instruction.  It would fit in a
reserved encoding, but it needs the bus to carry a locked
read-modify-write cycle, which is the one thing that makes a bus
arbiter hard, and it needs a compiler intrinsic.  The locks give
spinlocks, and spinlocks plus the doorbell give everything else, as
the next section says, so a swap instruction is shelved alongside
`BL` and `PUSH`: a reserved hole stays reserved until something has
been measured.

### Waiting for something: `BNV #6`

`BNV #6`, `WFI`: stop until an interrupt the CPU's mask admits is
pending, then continue (and take it, if the CPU is not already in
interrupt mode).  A non-negative operand, so an implementation
without it, msim today included, treats it as a no-op and the loop
around it still works.  A worker waiting for a doorbell, and core 0's
idle thread, sit in `WFI` rather than burn the bus.

### The rule: memory is coherent because there are no caches

Section 1.7 would say so.  A store is visible to every CPU before the
storing CPU's next instruction; loads are never reordered with each
other.  So a word in RAM written on one side and read on the other is
a correct channel with no barrier, and a Chairman lock is a correct
lock.  If an instruction cache is ever added it is a cache of ROM and
needs no coherence.  A data cache would void the rule and is not
planned.

## Deterministic timing: core-local memory

A worker bit-banging a protocol wants every instruction to take the
same time, and on a shared bus with SDRAM refresh it does not.  The
XMOS answer is memory the core does not share.  The proposal is a
small block RAM per core, 4 to 16 KB, visible twice: at chip select
30 as *the local memory of the CPU making the access*, so the same
code runs on any worker, and at chip select 29 at `cpu * 4 MB`, so
core 0 can load it.  A worker whose code, stack and buffers are in
its local memory never waits for the bus, and its timing is a
matter of counting instructions.  This is the one addition that
costs block RAM, the scarce resource on the boards in `hardware.md`,
and it is optional: a worker in shared RAM still works, just not to
the cycle.

## What Catflap does with it

As first proposed, before the section after this one simplified it.
From a process:

    int  core_start(int core, void (*fn)(void *), void *arg,
                    void *stack, size_t size);
    int  core_stop(int core);
    int  core_wait(int core);       /* until fn returns or is stopped */
    int  core_count(void);

`core_start` writes the trampoline table, sets the worker's mask to
the doorbell and whatever sources the caller asked for, and presses
control.  The worker runs `fn` with the process's `sb`, so it is in
the process's world: it can call every C library function that
touches no kernel state, `memcpy` and `strtol` and the maths, and
none that does, `printf` or `malloc` or anything with a file in it.
There is no trap to stop it; it is a convention, as it is on XMOS.
When `fn` returns, the trampoline rings core 0 and parks in `WFI`;
`core_wait` is a thread blocked on a semaphore the doorbell handler
signals.  A process exiting with workers running has them stopped.

Talking: a channel is a ring of words in shared RAM with the two ends
each owning one index, so it needs no lock.

    chan_t *chan_open(size_t words);
    void    chan_send(chan_t *, uint32_t);   /* either side */
    uint32_t chan_recv(chan_t *);

On core 0 the receiving side blocks the thread and the doorbell
wakes it; on a worker it polls, or `WFI`s on the doorbell for a
long wait.  That is XMOS's channel without the hardware.

Interrupts: a worker may be given a hardware source, a GPIO edge for
instance, which core 0 masks away from itself.  All cores share the
vector at 32, so its first instruction becomes a dispatch on `BNV #2`
through a table of per-core handlers; one load more on core 0's path.
The timer stays core 0's; a worker that wants time reads the IOC
counter, which it can do without anyone's leave.

Introspection: `/proc/cores`, one line per core: present, running,
owner process, entry address.

## Threads on the other cores

Could a core run threads and processes as core 0 does, with a thread
naming its core when it is made and never moving?  Yes, and it
complicates exactly one thing, which is worth seeing clearly.  This is
what was built: `catflap.md` section 4 says how, and the calls are
`thread_spawn_on`, `process_spawn_on` and `CF_CPU_EXCLUSIVE` in
`catflap.h`.

Today the kernel's data is consistent because only one flow of
control is ever inside the kernel: there is one core, and a tick that
lands while `in_kernel` is set is counted and its work done when the
call returns.  Kernel entry from a second core breaks that, and the
repair is a big kernel lock, Chairman lock 0, with one discipline: *a
core holds it exactly while its `in_kernel` is above zero*.  `kenter`
takes it on the step from 0 to 1, `kexit` drops it on the step back,
and a switch from a thread that was in the kernel to one that was not
drops it, and the reverse takes it, in `do_switch` where the two
counters are already exchanged.  The interrupt handler keeps its rule
of touching kernel data only when its own core's `in_kernel` is 0,
and then takes the lock first; it may spin, because no kernel code
waits for another core while holding the lock (the one wait in
`kexit` is for the core's own interrupt).  With one core the lock is
never contended and the build is what it is now.  The lock serialises
kernel work across cores, which for two to four cores on a micro is
the right trade: the cores are there for user-mode work.

The rest is mechanical:

- `struct cpu` becomes one per core and gains the run queues, the
  slice and `current`; `enqueue` puts a thread on its own core's queue
  and rings that core's doorbell if it is not this one, so a wakeup
  from core 0 reaches a thread on core 2 at once.  Sleepers, zombies
  and the list of all threads stay global under the lock.
- Every core wants its own timer, which settles question 1 below:
  the reload and value registers move into the core control block
  and source 31 is per-CPU, so each core's tick does its own slices
  and the scheduler on every core is today's scheduler.  Without
  that, core 0's tick would ring the others' doorbells on their
  behalf, which works but is a second mechanism.
- The doorbell handler on every core is `irq_dispatch`'s switch path;
  the interrupt bank's stack is per core.
- `__client_sb` and `this_cpu()` must be per core, and the shared C
  library reaches `__client_sb` through a fixed address in every
  static access it makes.  Indexing that by `BNV #2` would cost two
  instructions in every one of those sequences across the whole
  library.  Core-local memory at chip select 30 answers it for free:
  the per-core structure, `__client_sb` and the interrupt stack sit
  at the same address in every core's local memory, so `this_cpu()`
  is a constant again.  The local memory section above goes from
  optional to wanted, even at a few hundred bytes.
- A process has no core; its threads do.  `thread_create` takes one
  and `spawn` takes one for the new process's first thread, both
  defaulting to the caller's.  Everything a thread touches is shared
  memory already, there being no MMU, so processes need nothing
  else.  The C library's `malloc` and stdio are already one thread at
  a time by convention, and a second core changes nothing there.

And it simplifies the workers: a worker is a thread with an affinity
and one flag, *exclusive*, which masks the timer on that core and
refuses other threads, so its timing is its own.  `core_start` is
`thread_create` with the flag; the `wk_` conventions become advice
rather than a separate world; a worker that does call the kernel
merely spins on the lock and loses its determinism for the duration,
which is its choice.  One model instead of two.

The cost is a few hundred lines in the kernel, nothing in the
compiler, and a single-core build that is not changed.  Where the
first proposal spent its care keeping the kernel off the other cores,
this one lets it on and is simpler for it; the lock is what buys that.

## In msim

Done: `-n N` simulates N CPUs, one instruction each a cycle in bus-ID
order, and `-j seed` stalls them at random so that an interleaving
cannot be relied on; `-l KB` is the local memory.  `tests/sim/cpus.s`
exercises the Chairman's registers and `tests/os/cpus.c` the kernel.
Still to do: a worker driving a software UART on a GPIO line that msim
decodes, and the same worker in local memory with a cycle count that
must not vary.

## Questions

1. Per-core timers: settled, every CPU has one, in its control block.
2. One lock, Chairman lock 0, for the kernel; the other 31 are free.
3. A thread on any CPU may make threads on any other; nothing stops
   `spawn` from an exclusive CPU, and nothing recommends it.
4. Chip selects 29 and 30 are local memory: settled, in the reference.
5. An exclusive thread's stack and the code it runs are in shared RAM;
   local memory holds only the CPU's state.  Giving such a thread a
   stack in local memory, and copying its code there, is what would
   make its timing a matter of counting instructions.
