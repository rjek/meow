# MEOW

MEOW is a toy 32-bit RISC microcontroller with 16-bit instructions: sixteen
registers, two register banks, eight opcodes, and an ARM-flavoured assembler
syntax.  It exists to be simple enough to fit in a small FPGA and to be a
pleasant target for hand-written assembler and a C compiler.

## Layout

| Directory    | Contents |
|--------------|----------|
| `docs/`      | Architecture reference, ABI, assembler manual, design decisions |
| `isa/`       | `meow.isa`, the single source of truth for instruction encodings, and its generator |
| `lib/`       | Shared C library: generated encoding tables, disassembler, ELF support |
| `simulator/` | `msim`, the simulator and debugger |
| `assembler/` | The original Lua assembler, kept until the C assembler reaches parity |
| `attic/`     | Abandoned work: lcc port, libc, VHDL, Catflap OS.  Not maintained |

## Building

Plain `make` at the top level.  The simulator needs Lua 5.1 and libedit
(`liblua5.1-0-dev libedit-dev` on Debian and Ubuntu).
