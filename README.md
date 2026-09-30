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
| [docs/compiler.md](docs/compiler.md) | The C compiler: options, the code it writes and why, how it is tested |
| [docs/libc.md](docs/libc.md) | The C library: building against it, the platform layer, what was changed in the imports |
| [docs/lua.md](docs/lua.md) | Lua on MEOW: building it, running it, what it took |
| [docs/catflap.md](docs/catflap.md) | Catflap, the operating system: architecture, and how it was built |
| [docs/rom-size.md](docs/rom-size.md) | Where the bytes of the Catflap ROM are, what was done about them, and what is left |
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
| `libc/` | C library: PDCLib, musl's maths and number scanning, and the platform layers for `msim` and Catflap |
| `lua/` | Lua 5.4.7, built for MEOW and run under `msim`; optional, `make WITH_LUA=1` |
| `os/` | Catflap, the operating system: kernel in `os/kernel/`, programs in `os/bin/`; `make -C os run` boots it |
| `bench/` | Ten benchmark programs for the compiler; `make bench` |
| `tests/` | Regression tests for the assembler, linker, simulator, C compiler, C library, Lua and Catflap, and a random tester for the compiler |
| `attic/` | Abandoned work: the Lua assembler, lcc port, libc, VHDL, Catflap OS.  Not maintained |

## Building

```
make            # the tools, then the runtime, C library, Lua and Catflap
make check      # every test suite
make bench      # the compiler benchmarks
make docs       # the encoding diagrams in docs/reference.md, from meow.isa
```

The tools need a C99 compiler, and the simulator Lua 5.1 and libedit
(`liblua5.1-0-dev libedit-dev` on Debian and Ubuntu).  Everything that
is compiled for MEOW needs the C compiler, which is a separate
repository expected beside this one (next section); without it those
parts and their tests are skipped.

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
`../norcroft-ng/bin/nmcc`.  `tests/fuzz/run.sh FIRST LAST` tests the
compiler on random programs instead, one for each seed, with the host
compiler as the oracle; it is not part of `make check`.

`docs/compiler.md` has the options, the instruction sequences the
compiler uses for calls, constants and memory access and the reasons for
them, and what the random tester has found.

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
| bits | Masks, variable shifts, rotates, byte swaps | 1484457 | 420 |
| calls | Recursion with small frames and a few arguments | 2982775 | 376 |
| crc | CRC32 by table and bit by bit | 415447 | 312 |
| lists | A sorted linked list built, walked and freed | 2890665 | 424 |
| matmul | Nested loops, address arithmetic, `__mul` | 1569905 | 392 |
| sieve | Byte array indexing, inner loops with a stride | 1554816 | 176 |
| softfp | Floating point through the library | 12009617 | 524 |
| sort | Quicksort with an insertion sort tail | 1939813 | 624 |
| strings | Byte loops as the C library would write them | 816424 | 604 |
| vm | A bytecode interpreter: big switch, stack, pointers | 1100694 | 316 |
| total | | 26764613 | 4168 |

These are the baseline.  How it got there:

| Baseline | Instructions | Bytes |
|---|---|---|
| The first, before any optimisation work | 63000880 | 4996 |
| Strength reduction, constant hoisting, peepholes, inline 64-bit helpers; the runtime's multiply, divide and float multiply rewritten; `LDR` and `STR` at `[sp, #n]` | 27940656 | 4424 |
| Calls with the address in the next word, tail calls, register value tracking, writeback runs, shared pool words | 26764613 | 4168 |

58% fewer instructions and 17% less code than the first.

## The C library

`libc/` is a C library for programs compiled with `nmcc`: PDCLib (CC0)
for the standard library, musl (MIT) for the maths and for reading
numbers, and a platform layer for MEOW under `msim`.  `make` builds it into `libc/libc.a`, an archive that
`mld` takes members from as they are needed.  `docs/libc.md` says how to
compile and link against it, what the platform layer can and cannot do,
and what was changed in the imported sources.

## Lua

`lua/` is Lua 5.4.7, unmodified, compiled by `nmcc` against `libc/` into
`lua/lua.bin`.  It is an optional extra, there to show that the whole
system works: `make WITH_LUA=1` builds it, puts `/bin/lua` in the
Catflap ROM, and runs its tests.  `make -C lua run` then starts it
interactively under `msim` with a megabyte of RAM:

```
$ make -C lua run
Lua 5.4.7  Copyright (C) 1994-2024 Lua.org, PUC-Rio
> print(2^10, 7//2, ("x"):rep(3), os.time() > 0)
1024.0  3       xxx     true
```

`docs/lua.md` has the details, and `tests/lua/` runs scripts through it
and compares with what Lua 5.4 on the host prints, to the last digit of
every number.

## Catflap

`os/` is Catflap, an operating system for MEOW: preemptive threads,
processes, pipes, message queues and semaphores, romfs, devfs,
`/proc` and the host's files at `/host`, device drivers and file
systems that are ordinary programs, a shell and its tools, and Lua, all
calling one copy of the C library in a 360 KB ROM of which the kernel
is 27 KB.  `make -C os run` boots it under `msim` with a megabyte
of RAM and `os/` as `/host`:

```
$ make -C os run
Catflap: 1024 KB RAM
$ ls /bin | wc
22 44 316
$ echo "print(2^10)" > /tmp/t.lua
$ lua /tmp/t.lua
1024.0
$ free
1048576 bytes of RAM, 967160 free
$ memfs /mnt &
[8]
$ echo kept by a program > /mnt/note
$ cat /mnt/note
kept by a program
$ kill 8
$ ls /mnt
ls: /mnt: error 2
sh: ls: exit 1
[8] done, exit 137
```

`memfs` there is a file system in a program: it makes a port, mounts
itself, and answers the kernel's requests until it is killed.  `/tmp`
is the same program, started by `init` from `/etc/rc` at boot, which
is where servers a machine wants are listed.

`docs/catflap.md` describes it and `docs/rom-size.md` says where the
ROM's bytes go.

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
