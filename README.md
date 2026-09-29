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
| `tests/` | Regression tests for the assembler, linker, simulator and C compiler |
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
