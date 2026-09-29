# MEOW

MEOW is a toy 32-bit RISC microcontroller with 16-bit instructions: sixteen
registers in each of two banks, eight opcodes, and an ARM-flavoured
assembler syntax.  It exists to be simple enough to fit in a small FPGA and
to be a pleasant target for hand-written assembler and a C compiler.

## Documentation

| Document | Contents |
|---|---|
| [docs/reference.md](docs/reference.md) | Architecture reference: registers, instructions, memory map, Chairman |
| [docs/abi.md](docs/abi.md) | MABI, the C calling convention |
| [docs/assembler.md](docs/assembler.md) | The `mas` assembler and its language |
| [docs/linker.md](docs/linker.md) | The `mld` linker |
| [docs/simulator.md](docs/simulator.md) | The `msim` simulator and debugger |
| [docs/libc.md](docs/libc.md) | The C library: building against it, the platform layer, what was patched |
| [docs/lua.md](docs/lua.md) | Lua on MEOW: building it, running it, what it took |
| [docs/catflap.md](docs/catflap.md) | Catflap, a proposed operating system: architecture and plan |
| [docs/decisions.md](docs/decisions.md) | Why the specification says what it says |

## Layout

| Directory | Contents |
|---|---|
| `isa/` | `meow.isa`, the single source of truth for instruction encodings, and `isagen` |
| `lib/` | `libmeow`: generated encoding tables, disassembler, ELF reader and writer |
| `as/` | `mas` the assembler, `mdis` the disassembler, `mobjdump` |
| `ld/` | `mld` the linker |
| `rt/` | Runtime for compiled C: start-up code, multiply and divide, msim console output |
| `simulator/` | `msim` |
| `libc/` | C library: PDCLib, musl's maths and the MEOW platform layer |
| `lua/` | Lua 5.4.7, built for MEOW and run under `msim` |
| `tests/` | Regression tests for the assembler, linker, simulator, C compiler, C library and Lua |
| `attic/` | Abandoned work: the Lua assembler, lcc port, libc, VHDL, Catflap OS.  Not maintained |

## Building

`make` at the top level builds everything; `make check` runs the tests.
The simulator needs Lua 5.1 and libedit (`liblua5.1-0-dev libedit-dev` on
Debian and Ubuntu); everything else needs only a C99 compiler.  `make docs`
regenerates the encoding diagrams in the reference manual from `meow.isa`.

## C

The MEOW backend for the Norcroft-NG C compiler lives in that compiler's
repository as the `nmcc` tool (`make nmcc` there).  `nmcc -c` writes an
ELF object for `mld` and `nmcc -S` writes assembler for `mas`.  Link with
`rt/` (start-up code, multiply, divide, 64-bit integers, floating point
and the `msim` console) and `-d 0x08000000` so data lands in RAM:

```
nmcc -c -o hello.o hello.c
mld -f bin -d 0x08000000 -o hello.bin rt/crt0.o rt/mul.o rt/div.o rt/ll.o rt/softfp.o rt/msim.o hello.o
msim -q -r hello.bin
```

The C tests in `tests/c` do this both ways and run the result under
`msim`, checking a new test's output against the host compiler.  They are
skipped unless `NMCC` names the compiler or it is at
`../norcroft-ng/bin/nmcc`.

## Benchmarks

`make bench` compiles the programs in `bench/` with `nmcc`, runs them
under `msim -s` and prints the instructions each executed and the bytes
of code it compiled to, with the change since `bench/baseline.txt`.  Set
`NMCCFLAGS` to try compiler options, and rerun `bench/run.sh -b` to make
the current numbers the baseline.  Every program prints a checksum that
must match the host compiler's, so an optimisation that breaks the code
is a failure, not a speed-up.  `bench/profile.sh NAME` runs one program
under `msim -P` and lists the functions and instructions that took the
time.

| Program | Exercises | Instructions | Bytes |
|---|---|---|---|
| bits | Masks, variable shifts, rotates, byte swaps | 1499463 | 448 |
| calls | Recursion with small frames and a few arguments | 3236125 | 400 |
| crc | CRC32 by table and bit by bit | 415452 | 336 |
| lists | A sorted linked list built, walked and freed | 2890669 | 440 |
| matmul | Nested loops, address arithmetic, `__mul` | 1666700 | 404 |
| sieve | Byte array indexing, inner loops with a stride | 1554821 | 188 |
| softfp | Floating point through the library | 12797680 | 616 |
| sort | Quicksort with an insertion sort tail | 1951817 | 648 |
| strings | Byte loops as the C library would write them | 817195 | 616 |
| vm | A bytecode interpreter: big switch, stack, pointers | 1110734 | 328 |
| total | | 27940656 | 4424 |

These are the baseline as of the stack-relative `LDR` and `STR`.  The
first baseline, before any optimisation work, was 63000880 instructions
and 4996 bytes; the compiler's strength reduction, constant hoisting,
peepholes and inline 64-bit helpers, the runtime's rewritten multiply,
divide and float multiply, and the sp offset form brought it down 56%
in instructions and 11% in code.

## The C library

`libc/` is a C library for programs compiled with `nmcc`: PDCLib (CC0)
for the standard library, musl's maths (MIT) and a platform layer for
MEOW under `msim`.  `make` builds it into `libc/libc.a`, an archive that
`mld` takes members from as they are needed.  `docs/libc.md` says how to
compile and link against it, what the platform layer can and cannot do,
and what was changed in the imported sources.

## Lua

`lua/` is Lua 5.4.7, unmodified, compiled by `nmcc` against `libc/` into
`lua/lua.bin`.  `make -C lua run` starts it interactively under `msim`
with a megabyte of RAM:

```
$ make -C lua run
Lua 5.4.7  Copyright (C) 1994-2024 Lua.org, PUC-Rio
> print(2^10, 7//2, ("x"):rep(3), os.time() > 0)
1024.0  3       xxx     true
```

`docs/lua.md` has the details, and `tests/lua/` runs scripts through it
and compares with what Lua 5.4 on the host prints.

## A first program

```
        GET     msim.inc.s              ; from tests/sim: PUTC, HALT macros
start   ADR     r4, message
.loop   LDRB    r0, [r4], #1
        CMP     r0, #0
        BEQ     .done
        MOV     ir, r0
        BNV     #-6                     ; msim: print the character in ir
        B       .loop
.done   HALT
message DCB     "hello, world\n", 0
```

```
$ as/mas -I tests/sim -o hello.bin hello.s
$ simulator/msim -q -r hello.bin
hello, world
```
