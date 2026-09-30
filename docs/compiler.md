# The C compiler

`nmcc` is the Norcroft-NG C compiler with a MEOW backend.  It lives in
that compiler's repository, not this one: the backend is `ncc/meow/`
there, `make nmcc` builds `bin/nmcc`, and everything here that needs the
compiler looks for it at `../norcroft-ng/bin/nmcc` unless `NMCC` says
otherwise.  This document says how to use it, what the code it writes
looks like and why, and how it is tested.  `abi.md` has the calling
convention the code follows.

## Using it

```
nmcc -c -o hello.o hello.c              # an ELF object for mld
nmcc -S -o hello.s hello.c              # assembler for mas
```

| Option | Effect |
|---|---|
| `-c`, `-S` | Write an object, or assembler source |
| `-o file`, `-I dir`, `-D name` | As everywhere |
| `-std=c99`, `c11`, `c23` | The language.  The default is C90, and the C library's headers want C99 |
| `-Otime` | Favour speed over size; see below |
| `-Ospace` | Favour size.  The default is already close to it: 1.7% smaller on the benchmarks |
| `-zsb` | Reach static data through the word `__client_sb`, so that one copy of the code serves many copies of its data.  Catflap's shared C library and its programs are built this way; `catflap.md` section 6a says why |

The compiler has the RISC OS C headers built in and searches `-I`
directories first, so a program for the C library here is compiled with
`-Ilibc/pdclib/include -Ilibc/meow/include`, as `libc.md` describes.

## What the code looks like

MEOW gives a compiler less than most targets: a load or store takes its
address from one register with no offset, except for a word at
`[sp, #n]`; only `CMP` and `TST` set the flags; the one conditional
instruction is a branch that reaches 512 bytes; and the widest constant
an instruction carries is the twelve bits of `LDI`, which lands in `ir`.
Everything below is the backend working round one of those.  The
examples are what `nmcc -S` prints.

```
struct point { int x, y, z; };          sum     ADD      r1, r0, #4
                                                LDR      r2, [r1]
int sum(struct point *p)                        LDR      r1, [r0]
{                                               ADD      r1, r2
    return p->x + p->y + p->z;                  ADD      r0, #8
}                                               LDR      r0, [r0]
                                                ADD      r0, r1
                                                MOV      pc, lr
```

**Addresses.**  A field's address is formed in a register and then used.
The middle end is told that loads have no offsets, so it computes each
address as a value of its own, and common subexpression elimination
shares it between the accesses that want it.  Accesses at successive
addresses, the two words of a `double` or the fields of a structure
being copied, step one register with writeback, `LDR r2, [r3], #4`,
rather than form each address.  Locals and spilled values are reached
by `LDR` and `STR` at `[sp, #n]`.

**Constants.**  Zero is `EOR rd, rd`.  A constant of twelve bits is
`LDI #n; MOV rd, ir`, or just `LDI` when the next instruction can take
it from `ir`.  A wider one is built from an `LDI` with a shift and an
add where that is short enough, and is otherwise a word in a literal
pool.

**Literal pools.**  Addresses and wide constants are words placed after
the function, or between two of its instructions with a branch round
them when the function is longer than the 2 KB an `LDI` reaches:

```
        LDI      #22                    ; the pool word's distance
        ADD      ir, pc
        LDR      r1, [ir]               ; r1 = &scale
        ...
        DCD      scale
```

A word already in an earlier pool, or left behind by a call, is used
again when it is within reach backwards.

**What the registers hold.**  The backend remembers, from one
instruction to the next, which registers hold a known constant or a
known distance from one another.  A move between registers already
equal is not emitted, a constant already in a register is taken from
there, and a constant near one a register holds is made with one `ADD`.
A label forgets everything.

**Calls.**  There is no call instruction; a call sets `lr` and changes
`pc`.  The shortest sequence that reaches is used:

| Callee | Sequence | Bytes |
|---|---|---|
| Within 510 bytes, and already compiled | `ADD lr, pc, #4; B f` | 4 |
| Within 2 KB, and already compiled | `LDI #d; ADD lr, pc, #4; ADD pc, ir` | 6 |
| Anywhere | `ADD lr, pc, #4; LDR pc, [lr], #4; DCD f` | 8 |
| Anywhere, when the word would need padding and `f`'s address is in reach | `LDI #d; ADD ir, pc; ADD lr, pc, #4; LDR pc, [ir]` | 8 |
| Through a pointer | `ADD lr, pc, #4; MOV pc, r` | 4 |

The third is the common one.  The callee's address is the word after the
call: `lr` is pointed at it, the load through `lr` takes it into `pc`
and steps `lr` over it, and `lr` is the return address.  The word must
lie on a word boundary, so where the call does not, a halfword of
padding goes before it, ten bytes in all; the fourth form is what is
used instead when some earlier call or pool has left the address within
reach.  A function in another file, or later in this one, is never
"already compiled", because the backend emits code in one pass and the
linker does not shorten anything.

**Returns and tail calls.**  A function that saved `lr` returns by
popping it into `pc`, `LDR pc, [sp], #4`; one that did not returns by
`MOV pc, lr`.  A call that is the last thing a function does, with no
more than four argument words, becomes a jump once the frame is gone,
where that is shorter than a call and a return: always when `lr` was
never saved, and otherwise only when there is little to restore.

**Frames.**  There is no frame pointer.  On entry the function pushes
`lr`, if it makes a call, and the callee-saved registers it uses, a
halfword each, and lowers `sp` once for its locals; stack arguments for
calls are stored into space allocated then, not pushed.

**Branches.**  A forward branch is emitted short.  If its target has not
appeared by the time it is about to go out of reach, the backend plants
an island: a group of long jumps, `LDI #d; ADD pc, ir`, that the short
branches are pointed at instead.  A branch is carried by islands twice
at most, the second time as a jump that reaches anywhere, and branches
to one label share a slot.

**`switch`.**  A dense `switch` is a bounds check and a table of
one-halfword branches indexed by adding to `pc`; a sparse one is a tree
of compares.

**Multiplication and division.**  Multiplying by a constant is shifts,
adds and subtracts, over the signed digits of the constant so that a
run of ones costs one subtraction.  Everything else is a call: `__mul`,
`__udiv` and the rest of the runtime in `abi.md`, including division by
a constant, which measured faster through the library's divide than as
a multiplication by a reciprocal (`decisions.md`, 13).

**64-bit integers and floating point.**  Both are library calls, with
these done in line where the call would cost more than the work: 64-bit
add, subtract, negate, the bitwise operations, shifts by a constant,
comparisons for equality, and the conversions to and from `int`.

**Block copies.**  A structure assignment of up to eight words is
unrolled into loads and stores with writeback; a longer one is a loop
that counts by comparing addresses.

## `-Otime`

By default every choice above that trades size against speed goes to
size.  `-Otime` turns three of them round:

- The middle end is told that a load or store can take an offset of a
  byte either way, and the backend forms those addresses in `ir` just
  before the access.  That is more instructions than sharing the
  addresses as values, but it holds fewer registers across calls, so
  functions save and restore fewer.
- A call takes the two-instruction form whether or not its word needs
  padding.
- A tail call is always taken.

| | Default | `-Otime` | |
|---|---|---|---|
| Catflap ROM | 346,672 | 362,608 | +4.6% |
| Lua `fib(25)`, instructions | 196.1 M | 177.8 M | -9.3% |
| Lua sieve test, instructions | 14.4 M | 14.2 M | -1.4% |
| The ten benchmarks, instructions | 26,764,613 | 26,727,510 | -0.1% |
| The ten benchmarks, bytes | 4,168 | 4,192 | +0.6% |

The benchmarks are small leaf-heavy loops and do not care; Lua, which
is calls all the way down, does.  The repository builds everything at
the default, because the ROM is what is short.

## Limits

- A function may have about 32,000 virtual registers, which a couple of
  thousand lines of dense code in one function reach.  Beyond that the
  compiler stops with "Register heap overflow".
- The backend emits code in one pass: a call or branch forward never
  uses a shorter form than the one that reaches anywhere, and the
  linker relaxes nothing.
- `long double` is `double`.  `char` is unsigned.

## How it is tested

| What | Where | Oracle |
|---|---|---|
| The language, feature by feature | `tests/c/`, run by `make check` | The host compiler's output for the same program, recorded when the test is added |
| What the code costs | `bench/`, run by `make bench` | Instructions and bytes against `bench/baseline.txt`; each program's checksum against the host's |
| The C library, Lua and Catflap built with it | `tests/libc/`, `tests/lua/`, `tests/os/` | The host's C library and the host's Lua |
| Random programs | `tests/fuzz/` | The host compiler, program by program |

`tests/fuzz/run.sh FIRST LAST` is the last of those and is not part of
`make check`.  For each seed `gen.py` writes a program of a dozen
functions of random expressions, loops, switches, structure copies,
64-bit and floating arithmetic and calls, which prints one checksum.
The program is built by the host compiler, which says what the answer
is, and by `nmcc`, and run under `msim`.  `JOBS=16` runs sixteen seeds
at once; options after the seeds go to `nmcc`, so `run.sh 1 1000
-Otime` tests that.  A program that fails is left in
`tests/fuzz/failed/`, and `reduce.py` cuts it down, statement by
statement and then block by block, to the hundred lines or so that
still fail; `one.sh` compares one file.  The host build used for
reducing traps on undefined behaviour, so a reduction cannot wander
into a program whose answer means nothing.

The first 2,000 seeds found a dozen faults.  All but one are now cases
in `tests/c/fuzz.c` and `tests/c/bigfn.c`; the exception could not be
made to fail to order.  All but one, again, were in Norcroft's
machine-independent front and middle end, and so were wrong for ARM
too:

- `203 != (a >= b)` was simplified as if 203 were 1.
- `(c ? 5 : x) == 5` looked at the wrong arm for the constant.
- `(-a) * n` with `n` negative became `a * n`.
- `(a >> 17) >> 29` became a shift by 46, which a later rule took for a
  shift by 14.
- `(a / k) / k1` became `a / (k * k1)` when the product overflowed.
- A `?:` lifted out of a loop let what one arm had computed be used
  after the join, where it might never have been computed.
- A `?:` within a `?:`, lifted, was split into blocks in the wrong
  order, and as the second arm left its value in the wrong register.
- A load known to fetch the constant just stored there, when also a
  common subexpression kept in memory, was never put in memory.
- A function with more than 15,800 virtual registers failed on a
  64-bit host, where the allocator's tables are twice the size.
- The register allocator, deciding that a division whose result is not
  used still needs its test for zero, kept the address of a local
  structure after the block it was declared in had ended.  This is the
  one without a test: whether it goes wrong depends on how the host
  compiler lays out the allocator's stack.

The last was the backend's: a structure copy long enough to be a loop
changes the flags, and the middle end, not told so, moved one between a
compare and its branch.  4,000 seeds at the default and 1,500 with
`-Otime` now pass.
