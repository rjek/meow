# The size of the ROM

The Catflap ROM was 460 KB when it first held everything: a kernel, a
shell, a handful of tools, the whole C library and Lua.  RISC OS 2
fitted a kernel, window manager, filing systems, sprites, fonts and BBC
BASIC into 512 KB, so that was not a good look, and this document began
as a review of where the bytes were and who was to blame.  The blame
did not fall where the size suggested, two of the three things the
review recommended have since been done, and the ROM is 360 KB with
more in it than it had.

| | ROM, bytes | |
|---|---|---|
| As reviewed | 459,676 | |
| The C library's implementations chosen for this machine | 376,384 | -18% |
| The compiler's calls, constants and register tracking | 342,980 | -9% |
| A `strtod` that is right | 346,672 | +1% |
| Ports, poll and kill in the kernel; `memfs`, `kill` and their test | 359,664 | +4% |

No function was lost along the way: every one in C99's library is
still in the ROM for a program built later to call.

## Where the bytes are

| Part | Bytes | Share |
|---|---|---|
| Lua 5.4 | 178,384 | 50% |
| Maths: musl's, the fdlibm exponentials, the float wrappers | 69,216 | 19% |
| PDCLib: stdio, `printf` and `scanf`, strings, time and the rest | 37,924 | 11% |
| The kernel | 26,840 | 7% |
| The other programs and the romfs directory | 19,084 | 5% |
| Soft float and the 64-bit runtime | 10,774 | 3% |
| The platform layer, `malloc`, the calendar | 9,036 | 3% |
| Reading numbers: musl's scanner | 5,784 | 2% |
| Initialised data and the library's relocation list | 2,626 | 1% |

The operating system proper, the kernel with its scheduler, six file
systems, pipes, IPC, `/proc`, and the ports through which programs
serve devices and file systems of their own, is 27 KB; it was 21 KB
before the ports, which with `vfs_poll` and `process_kill` are 5.5 KB.  The 1980s systems being
compared with were that size or smaller only as kernels: MS-DOS 2 was
about 30 KB, the BBC MOS 16 KB, the Unix V7 kernel 50 KB.  RISC OS 2's
512 KB held far more than its kernel, and held it as hand-written
32-bit ARM code.  Catflap's kernel was never the problem.  The C
library was 47% of the ROM and Lua 43%, and both were bigger than they
should have been for two reasons: the library's implementations were
chosen for a 64-bit server, and the compiler's code was half as big
again as it might be.

## The C library: from 217 KB to 122 KB

The library was imported whole, on the principle that a binary built
after the ROM must find every standard function in it.  That principle
is right and stays.  It does not require the functions to be the
implementations musl chose, and these went:

- **Float variants**, 43 KB.  `sinf` and the other hundred `float`
  functions are full separate implementations in musl.  On a machine
  whose `float` and `double` arithmetic are both software, `sinf(x)` as
  `(float)sin(x)` is no slower and is correctly rounded more often than
  musl's own.  They are wrappers now, 1.7 KB.
- **Time zone code**, 12.7 KB.  PDCLib carries the full Olson zoneinfo
  parser, and there is no zoneinfo file for it to parse.  A fixed-UTC
  `gmtime`, `localtime` and `mktime` is 2.4 KB, and each process's copy
  of the library's data fell from 16 KB to 6 KB, since the zone state
  went with the zone code.
- **Non-standard maths**, about 15 KB.  The Bessel functions, `exp10`,
  `scalb`, `significand`, `drem`, `finite` and their variants are POSIX
  and BSD extras, not C.
- **Table-driven exponentials and logarithms**, about 20 KB.  musl's
  `exp`, `log`, `log2` and `pow` are the ARM optimized-routines
  versions, tuned for a machine with a floating point unit and cache,
  each with a 2 to 4 KB table.  The fdlibm versions musl used until
  2018 have no tables and are a quarter the size, and on soft float are
  not measurably slower.
- **dlmalloc**, 13 KB, before the review began.  The allocator from
  Kernighan and Ritchie's book is 600 bytes; a program that allocates
  and frees tens of thousands of objects runs up to 1.8 times slower,
  and nothing here is that program.

Those took the ROM to 376 KB.  The last 10 KB of the library's fall is
the compiler's doing, below.

What stays, though a 1980s library would not have had it, is exact
conversion of numbers: `printf`'s through PDCLib's big integers, 15 KB
with the formatting engine, and `strtod`'s through musl's scanner,
6 KB.  A 1980s `printf` was 2 KB and wrong in the last place.  Lua
wants a number to survive being printed and read back, and the review
assumed PDCLib's `strtod` was as exact as its `printf`; it was not
(`libc.md`), and putting that right is the 3.7 KB the ROM has grown
since.

## The compiler: from 46% bigger than Thumb to 32%

The same Lua sources compiled with clang for Thumb-1, the other 16-bit
instruction set for a 32-bit machine, give a direct measure:

| Compiler and target | Lua library code |
|---|---|
| clang, Thumb-1, `-Os` | 121,716 |
| clang, Thumb-2, `-Os` | 125,571 |
| nmcc, MEOW, now | 160,732 |
| clang, Thumb-1, `-O2` | 166,923 |
| nmcc, MEOW, as reviewed | 177,500 |
| clang, ARM, `-Os` | 179,299 |

When reviewed, MEOW code was 1.46 times the size of Thumb-1 at `-Os`,
and the ratio was the same for every file, 1.4 to 1.7: not one
construct going wrong but the ordinary cost of everything.  Side by
side on a small function the compiler's code was as good as clang's
instruction for instruction.  What differs is what an instruction can
do.  A call was 8 bytes plus a 4-byte pool word against Thumb's 4-byte
`BL`.  Saving four registers and the link is five halfwords against
Thumb's one `PUSH`.  Reading a structure field is `ADD rd, rs, #8; LDR
rd, [rd]` against one `LDR rd, [rs, #8]`.  Loading a small constant is
`LDI; MOV` against one `MOVS`.

The review thought the compiler could find 20 KB and no more.  It found
33 KB, by doing better at living with those four than the review
thought possible (`compiler.md` describes each):

- **Calls.**  The callee's address sits in the word after the call,
  where `lr` finds it, instead of in a pool that an `LDI` must find
  first: two instructions rather than four, and no pool word apart from
  the one in line.  Calls to a function already compiled and near by
  are shorter still, and a call that ends a function is a jump.
- **Knowing what the registers hold.**  The backend tracks constants
  and the distances between registers across straight-line code, so a
  constant or address already somewhere is not made again.  The 435
  `MOV a, b; MOV b, a` pairs are 8; the 4,279 `LDI; MOV` pairs are
  3,081.
- **Writeback.**  Successive fields and the two words of a `double` are
  reached by stepping one register.
- **Pool words used again** when an earlier pool or call left one in
  reach, and **branch islands** that share a slot between branches to
  one label.

Classifying every halfword of code in the ROM, as it was at 347 KB:

| Pattern | Bytes | Share of code |
|---|---|---|
| Calls: 7,900 of them, 7.8 bytes each on average | 61,514 | 20% |
| `MOV` | 37,688 | 12% |
| `ADD` and `SUB`, much of it forming addresses for loads | 33,714 | 11% |
| Branches | 30,484 | 10% |
| Loads and stores through a register | 30,106 | 10% |
| Pushes and pops, one halfword per register | 23,196 | 8% |
| `CMP` and `TST` | 21,772 | 7% |
| Loads and stores at `[sp, #n]` | 16,422 | 5% |
| Constants, `LDI #k; MOV rd, ir` | 12,324 | 4% |
| Addresses of strings, `LDI; MOV rd, pc; ADD rd, ir` | 9,516 | 3% |
| Pool loads, `LDI; ADD ir, pc; LDR rd, [ir]` | 6,456 | 2% |
| The rest: shifts, logic, case tables, returns | 25,976 | 8% |

What the compiler could still do:

- **Save fewer registers.**  Functions that save the link save 4.5
  registers on average, 1,346 such functions.  Allocation that prefers
  the argument registers in leaf-like code, and shrink-wrapping so that
  the early return in a function saves nothing, would save perhaps
  8 KB.  This is allocator work in the middle end.
- **Shorten calls forward within a file.**  A call to a function
  defined later in the same file takes the 8-byte form because the
  backend emits code in one pass.  A second pass, or relaxation in the
  linker, would save about 3 KB.

## Lua

Lua is 178 KB where Thumb-1 would make it 135 KB.  Its own knob,
`LUA_32BITS`, was measured at 15% smaller and rejected because it
changes the language (`lua.md`).  Nothing else in Lua is optional.  Lua
at Thumb density would be the size of BBC BASIC V plus its own
compiler, which is what it is.

## The instruction set, as a last resort

Each of the following is a single, regular instruction of the kind
every 16-bit instruction set for a 32-bit machine has, none needs a
new addressing mode in hardware beyond what `LDR [sp, #n]` already
proved, and each pays for itself in the measurement above:

| Instruction | What it replaces | Sites | Saving |
|---|---|---|---|
| `BL` with a signed halfword offset, two halfwords as Thumb does it | the call sequences, 61 KB | 7,900 | about 30 KB |
| `PUSH` and `POP` with a register list | one halfword per register | 6,055 pushes, 5,543 pops | about 17 KB |
| `LDR` and `STR` with `[Rn, #imm5*4]`, extending the `sp` form to any base | `ADD` then the access | 2,900 | about 8 KB |
| `MOV rd, #imm8` | `LDI; MOV` | 3,081 | about 6 KB |
| `LDR rd, [pc, #n]` | the three-halfword pool load | 1,076 | about 4 KB |

`ADDS` and `SUBS` were measured and rejected for speed (`decisions.md`,
12); for size they would take about half of the 22 KB of compares.  The
five above are worth about 65 KB, a fifth of the ROM, and would put
MEOW close to Thumb-1's density.  `BL` alone is 30 KB and is the one a
hardware designer would add first: it is what makes a 16-bit
instruction set usable for C, and its absence is why a fifth of every
program is call sequences.  The PC-relative load was worth 25 to 35 KB
when calls fetched their addresses from pools; now that they do not, it
is the least of the five.  (Measured again on the 103 KB ROM, after calls became the eight-byte
inline-word form and Lua and the maths left, `BL` is worth 6 KB and
`PUSH`/`POP` 5 KB: `decisions.md` item 17 has the encodings and the
figures, and they are shelved until the system has run on hardware.)

## What is left, in order

1. The compiler: register saving and forward calls.  About 11 KB, ROM
   about 350 KB.
2. Then decide about `BL`, `PUSH` and `POP`, on the basis that they are
   worth 47 KB between them and that the reserved encoding space is
   there for exactly this.  ROM about 300 KB.
3. The offset load, the byte constant and the PC-relative load, 18 KB
   more, if the encoding space is still not wanted for anything else.

## Reassessed without Lua

Lua was only ever the demonstration that the whole system worked, and
is now an option, `make WITH_LUA=1`, rather than part of the ROM.  The
default ROM is 181 KB, and this is what is in it:

| Part | Bytes | Share |
|---|---|---|
| maths: musl's double functions and the fdlibm five | 67,512 | 37% |
| kernel | 26,840 | 15% |
| programs: sh, ls, cat, ps and the rest, and the two test programs | 16,552 | 9% |
| PDCLib's printf, scanf and strtod engines, exact to the last digit | 15,448 | 9% |
| soft float and 64-bit runtime | 10,774 | 6% |
| the rest of PDCLib: stdio, stdlib, string, time, ctype | 22,000 | 12% |
| the platform layer, malloc, the float wrappers, UTC time | 10,720 | 6% |
| romfs and the data image | 10,000 | 6% |

Three things stand out.  First, a third of the ROM is `<math.h>`,
which the shell and every tool in it never call: it is there for the
binary built later, as agreed.  Second, the kernel is 27 KB with
everything a small Unix has, and needs no apology.  Third, at this size
the instruction set changes in the section above are worth a quarter of
the ROM, as before, and would bring it to about 135 KB, which is the
size of RISC OS 2's kernel module alone.  Against a 512 KB budget the
default ROM now leaves 330 KB, enough for Lua and a good deal else.

## Reassessed again: 104 KB

Two more cuts, neither losing a function.  The test programs are built
for the tests and left out of the ROM.  And `<math.h>` is no longer in
the ROM at all: it is `os/obj/libm.a`, an archive each program links
what it calls from, exactly as a program built later would.  The ROM
keeps only the five maths functions its own `strtod` needs and the
classification functions behind the macros.  What is left:

| Part | Bytes |
|---|---|
| kernel | 27 K |
| PDCLib: stdio, stdlib, string, time, ctype, and the exact printf, scanf and strtod | 38 K |
| soft float and 64-bit runtime | 11 K |
| platform layer, malloc, UTC time, and what strtod needs of the maths | 10 K |
| data image, relocation list, romfs metadata | 8 K |
| programs: sh, ls, cat, ps, free, mount, uname, mkdir, rm, wc, sleep, uptime, kill, memfs, init, echo | 10 K |

A shell, sixteen tools, four file systems, pipes, IPC, `/proc`, and a
complete C library at 104 KB, on a compiler that spends 1.46 bytes for
every one Thumb spends.  The instruction set changes above would bring
it to about 75 KB.

`ramfs` has since left the kernel too: `/tmp` is the `memfs` server,
started by `init` from `/etc/rc`.  103 KB.
