# Why the ROM is 460 KB, and what would make it smaller

A review, with measurements, of the Catflap ROM as it stood at 460 KB.
RISC OS 2 fitted a kernel, window manager, filing systems, sprites,
fonts and BBC BASIC into 512 KB, so 460 KB for a kernel, a shell, a
handful of tools and Lua is not a good look.  The measurements below
say where the bytes are and who is to blame, and the blame does not
fall where the size suggests.

## Where the bytes are

| Part | Bytes | Share |
|---|---|---|
| Lua 5.4 | 197,216 | 43% |
| musl maths: double | 90,580 | 20% |
| musl maths: float variants | 42,816 | 9% |
| musl maths: tables | 14,216 | 3% |
| PDCLib: everything else | 66,236 | 14% |
| PDCLib: time zone code | 12,668 | 3% |
| kernel | 23,200 | 5% |
| soft float and 64-bit runtime | 11,598 | 3% |
| the other programs | 12,000 | 3% |

The operating system proper, the kernel with its scheduler, four file
systems, pipes, IPC and `/proc`, is 23 KB.  The 1980s systems being
compared with were that size or smaller only as kernels: MS-DOS 2 was
about 30 KB, the BBC MOS 16 KB, the Unix V7 kernel 50 KB.  RISC OS 2's
512 KB held far more than its kernel, and held it as hand-written
32-bit ARM code.  Catflap's kernel is not the problem.  The C library
is 47% of the ROM and Lua 43%, and both are bigger than they should be
for one reason, which is the density of the code the compiler produces.

## The compiler: 46% bigger than Thumb

The same Lua sources compiled with clang for Thumb-1, the other 16-bit
instruction set for a 32-bit machine, give a direct measure:

| Compiler and target | Lua library code |
|---|---|
| clang, Thumb-1, `-Os` | 121,771 |
| clang, Thumb-2, `-Os` | 125,571 |
| clang, Thumb-1, `-O2` | 166,923 |
| nmcc, MEOW | 177,500 |
| nmcc, MEOW, `-Ospace` | 175,956 |
| clang, ARM, `-Os` | 179,299 |

MEOW code is 1.46 times the size of Thumb-1 at `-Os`, and the ratio is
the same for every file, 1.4 to 1.7, so it is not one construct going
wrong but the ordinary cost of everything.  `-Ospace` changes 1%: the
compiler's default output is already its size-conscious output.
Classifying every halfword in the ROM says what the ordinary cost is:

| Pattern | Bytes | Share of code |
|---|---|---|
| calls through the literal pool, `LDI; ADD ir, pc; ADD lr, pc, #4; LDR pc, [ir]` | 68,904 | 17% |
| branches | 56,454 | 14% |
| `MOV` | 53,188 | 13% |
| `ADD` and `SUB`, much of it forming addresses for loads | 47,592 | 12% |
| loads and stores | 36,028 | 9% |
| `CMP` and `TST` | 23,768 | 6% |
| pushes and pops, one halfword per register | 25,880 | 6% |
| constants, `LDI #k; MOV rd, ir` | 17,116 | 4% |
| the rest | 73,000 | 18% |

Side by side on a small function, the compiler's code is as good as
clang's instruction for instruction.  What differs is what an
instruction can do.  A call is 8 bytes plus a 4-byte pool word against
Thumb's 4-byte `BL`.  Saving four registers and the link is five
halfwords against Thumb's one `PUSH`.  Reading a structure field is
`ADD rd, rs, #8; LDR rd, [rd]` against one `LDR rd, [rs, #8]`.  Loading
a small constant is `LDI; MOV` against one `MOVS`.  Those four account
for roughly 100 KB across the ROM, and no compiler can remove them.

What the compiler can do, with the sizes measured or estimated:

- **Resolve same-file calls to the 6-byte form.**  751 of the 4,517
  pool words in Lua, PDCLib and the kernel name something in the same
  object; a call to a function defined later in the file goes through
  the pool because the backend does not know where it will land.  A
  second pass, or linker relaxation, would save about 3 KB.
- **Save fewer registers.**  Functions that save the link save 4.3
  registers on average, 1,588 such functions.  Allocation that prefers
  the argument registers in leaf-like code, and shrink-wrapping so that
  the early return in a function saves nothing, would save perhaps
  8 KB.
- **Coalesce moves.**  435 `MOV a, b; MOV b, a` pairs, and many of the
  26,000 moves marshal arguments that a better allocation would place
  where they are needed: perhaps 8 KB.
- **Constants.**  4,106 of the 4,279 `LDI; MOV` pairs load a value
  under 256.  Nothing to be done without an instruction, since `EOR
  rd, rd; ADD rd, #k` is also two halfwords.

About 20 KB in all, 5%.  The compiler is not where the fat is either.

## The C library: 217 KB, most of it never called

The library was imported whole, on the principle that a binary built
after the ROM must find every standard function in it.  That principle
is right and stays.  It does not require the functions to be the
implementations musl chose for a 64-bit server:

- **Float variants as wrappers**, 43 KB.  `sinf` and the other 100 or
  so `float` functions are full separate implementations in musl.  On
  a machine whose `float` and `double` arithmetic are both software,
  `sinf(x)` as `(float)sin(x)` is no slower and is correctly rounded
  more often than musl's own.  Every function stays.  Saving: about
  40 KB.
- **Time zone code**, 12.7 KB.  PDCLib carries the full Olson zoneinfo
  parser, and there is no zoneinfo file for it to parse; `localtime` is
  UTC.  A fixed-UTC `localtime`, `mktime` and `tzset` keeps every
  function and takes a few hundred bytes.  Saving: about 12 KB.
- **Non-standard maths**, about 15 KB.  The Bessel functions `j0`, `j1`,
  `jn`, `y0`, `y1`, `yn`, and `exp10`, `scalb`, `significand`, `drem`,
  `finite` and their variants are POSIX and BSD extras, not C.  "The
  whole C library" does not need them.
- **Table-driven exponentials and logarithms**, 14 KB of tables plus
  code.  musl's `exp`, `log`, `log2` and `pow` are the ARM
  optimized-routines versions, tuned for speed on a machine with a
  floating point unit and cache, each with a 2 to 4 KB table.  The
  fdlibm versions musl used until 2018 have no tables and are a
  quarter the size, and on soft float are not measurably slower.
  Saving: about 20 KB.
- **The exact floating point printer and scanner**, 14 KB.  PDCLib's
  `printf` and `strtod` are correct to the last digit through big
  integer arithmetic.  A 1980s `printf` was 2 KB and wrong in the last
  place; Lua wants the exact one for round trips.  Keep it.

Together about 85 KB, taking the library from 217 KB to 130 KB, with no
function lost.  Done: the ROM went from 460 KB to 377 KB, the library
in the kernel image from 240 KB to 145 KB, and each process's copy of
the library's data from 16 KB to 6 KB, since the zone state went with
the zone code.  The maths tests still match the host to six digits.

## Lua

Lua is 197 KB where Thumb-1 would make it 140 KB.  Its own knob,
`LUA_32BITS`, was measured earlier at 15% smaller and rejected because
it changes the language.  Nothing else in Lua is optional.  Lua at
Thumb density would be the size of BBC BASIC V plus its own compiler,
which is what it is.

## The instruction set, as a last resort

Each of the following is a single, regular instruction of the kind
every 16-bit instruction set for a 32-bit machine has, none needs a
new addressing mode in hardware beyond what `LDR [sp, #n]` already
proved, and each pays for itself in the measurement above:

| Instruction | What it replaces | Sites | Saving |
|---|---|---|---|
| `BL` with a signed halfword offset, two halfwords as Thumb does it | the 8-byte pool call and its pool word | 8,600 | about 40 KB |
| `PUSH` and `POP` with a register list | one halfword per register | 6,776 pushes, as many pops | about 20 KB |
| `LDR` and `STR` with `[Rn, #imm5*4]`, extending the `sp` form to any base | `ADD` then `LDR` | 5,000 or so | about 10 KB |
| `MOV rd, #imm8` | `LDI; MOV` | 4,100 | about 8 KB |
| `LDR rd, [pc, #n]`, already recorded in `decisions.md` | the three-halfword pool load | 9,852 | 25 to 35 KB |

`ADDS` and `SUBS` were measured and rejected for speed; for size they
would take about half of the 24 KB of compares.  The five above are
worth about 100 KB, a quarter of the ROM, and would put MEOW at
Thumb-1's density, which is the density this compiler's output
deserves.  `BL` alone is 40 KB and is the one a hardware designer would
add first: it is what makes a 16-bit instruction set usable for C, and
its absence is why 17% of every program is call sequences.

## What to do, in order

1. The library: float wrappers, fixed UTC, drop the BSD and POSIX
   extras, fdlibm exponentials.  85 KB, no instruction changes, no
   function lost.  ROM about 375 KB.
2. The compiler: same-file calls, register saving, move coalescing.
   20 KB.  ROM about 355 KB.
3. Then decide about `BL`, `PUSH` and `POP`, and the offset load, on the
   basis that they are worth 70 KB more and that the reserved encoding
   space is there for exactly this.
