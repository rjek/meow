# msim: the MEOW simulator

`msim` runs a MEOW machine: one or more CPUs, ROM, RAM and the Chairman
system controller, with a command-line debugger.  It needs Lua 5.1 and libedit to
build (`liblua5.1-0-dev libedit-dev` on Debian and Ubuntu).

```
msim [-vhiqs] {-f spec | -r rom [-m KB]} [-n CPUs] [-l KB] [-j seed] [-H dir]
     [-c cycles] [-P file]
```

| Option | Meaning |
|---|---|
| `-r rom` | Load a flat image as ROM at chip select 0, with RAM at chip select 1, the IOC at 2 and the Chairman at 31 |
| `-m KB` | How much RAM `-r` provides, in KB; 64 unless told otherwise.  A program finds the figure in the Chairman's chip-select table, which is how `crt0` places the stack |
| `-n CPUs` | How many CPUs, 1 to 32; one unless told otherwise.  CPU 0 runs from reset and the others wait to be started through the Chairman's control blocks (reference section 5.5) |
| `-l KB` | Give every CPU this much local memory, at chip select 30 as its own and at 29 as all of them 4 MB apart; none unless told otherwise |
| `-j seed` | Stall CPUs at random, one cycle in four from a generator seeded with this, so that a program's independence of their interleaving can be tested; the same seed gives the same run |
| `-G n,baud` | Watch IOC GPIO line `n` as the output of a software UART, 8 data bits, no parity, one stop bit, at `baud`, and print the bytes it carries; a framing error is reported on standard error |
| `-D image` | Put an SD card on the IOC's SPI master, holding this file, which is read and written in place; its size, rounded down to whole 512 KB, is the card's |
| `-T file` | Write a line per instruction CPU 0 executes: its address and word, then the sixteen registers of the active bank and the sixteen of the other, all after it, in hex.  The hardware's testbench reads this (`hw/`) |
| `-H dir` | Lend a host directory to the program through `BNV #-18`.  Catflap mounts it at `/host` |
| `-f spec` | Describe the machine in a spec file instead (below) |
| `-c cycles` | Stop after this many instructions.  Otherwise run until the program halts |
| `-v` | Trace: print every instruction as it executes, with the registers after it |
| `-i` | Interactive: start the debugger instead of running |
| `-q` | No banner |
| `-s` | Print the number of instructions executed to standard error on exit, for benchmarking |
| `-P file` | On exit write one `address count` line for every instruction address executed (in the low 1 MB); `bench/profile.sh` turns this into a profile |

The exit status is the value passed to the halt call (below), or 0 after
`-c` cycles.

With more than one CPU a cycle is one instruction on every running CPU
that is not waiting in `WFI`, in bus-ID order, and then one tick of the
devices, in chip-select order, so that a source the IOC raises in its
tick is seen by the Chairman's in the same cycle, as the hardware sees
it before the next instruction; `-c` counts cycles, `-s` counts
instructions on every CPU.  The
trace names the CPU on each line.  The Chairman is modelled as the
reference describes it: a pending word and a timer per CPU, the control
blocks, the doorbell and the locks, with the lock's read and set being
one access because the simulator is one program.  `BNV #6` stops a CPU
until the Chairman's tick finds something pending that its mask admits.
The debugger works on CPU 0; stepping it steps the machine.

## Spec files

A spec file has one `chip` line per chip select; blank lines and lines
starting with `;` are ignored.

```
chip 0 rom firmware.bin
chip 1 ram 65536
chip 2 ioc
chip 31 sys
```

`rom` takes a file name; `ram` takes a size in bytes (default 128 MB);
`sys` is the Chairman; `ioc` is the IOC, which takes the console from
the Chairman and so should follow it.

## The IOC

The IOC of the reference's section 6: identification and clock; two
UARTs, with 16-byte FIFOs; the SPI master; 32 GPIO lines with their
edge interrupts (Chairman source 3); the real-time clock, whose
seconds start from the host's clock, with its alarm (source 4) and the
counter, which is the cycle count at the timer's 1 MHz; and system
control, where halt 1 is `WFI` and halt 2 ends the run.

UART 0 is the console, standard input and output, and the Chairman's
serial registers are then absent, as the reference says: they read as
0 and a write is refused with a message.  The end of standard input is
reported as a break in the status register, once, which the clear
register clears.  Standard input is looked at when the status register
is read and, once the UART has been touched at all, every 4096 cycles
besides, so a reader driven by the UART's interrupt sees it too; a
program that reads its input through `BNV #-12` instead is left alone.
UART 1 receives what it sends, a loopback.
A byte sent goes to the host at once, but the transmit FIFO empties at
the divisor's rate, ten bit times a byte, so the status register's
room and idle bits, and the transmit interrupt, behave as they would;
a byte written with no room is dropped, as the reference says.

The SPI master takes 16 clocks times the divisor plus one for a byte,
and is busy for that long, during which a write to the data register
is ignored, as the reference says; the completion interrupt (source 2)
comes if asked for.  With `-D` an SD card is on it, selected by the
control register's chip-select bit: a high-capacity card in SPI mode,
answering CMD0, CMD8, CMD55 and ACMD41 (ready at the second asking),
CMD58, CMD9 and CMD10, CMD16, CMD17 and CMD24, with the usual tokens,
one byte of delay before a response and two bytes of busy after a
write; multiple-block commands are illegal.  Without a card the data
line reads 0xff.  `os/bin/sd.c` is a driver for it, and `os/tools/mkfat`
makes a FAT16 or FAT32 image with files on it for `os/bin/fatfs` to
mount.

GPIO inputs read as 0, since nothing drives them, and outputs read
back.  `-G` is how a CPU bit-banging a serial line out of the GPIO is
tested.

## Extension calls

The simulator implements the architecture-defined `BNV` operations and
these negative, simulator-specific ones.  They are the usual way for test
programs to talk to the outside world.

| Call | Effect |
|---|---|
| `BNV #-2` | Halt.  The process exits with the low byte of `ir` as its status |
| `BNV #-4` | Dump both register banks to standard output |
| `BNV #-6` | Write the low byte of `ir` to standard output as a character |
| `BNV #-8` | Write `ir` to standard output as a signed decimal number |
| `BNV #-10` | Write `ir` to standard output in hexadecimal |
| `BNV #-12` | Read one character from standard input into `ir`, or -1 at its end.  Standard output is flushed first |
| `BNV #-14` | The host's time in seconds since 1970 into `ir` |
| `BNV #-16` | The number of instructions executed so far into `ir` |
| `BNV #-18` | The directory lent with `-H`: `r0` is the operation (0 probe, 1 open, 2 close, 3 read, 4 write, 5 stat, 6 readdir, 7 mkdir, 8 unlink), `r1` to `r4` its arguments, and `ir` the result or a negative `errno`.  Without `-H` every operation answers `-ENOSYS`.  Paths are relative to the directory and may not contain `..`; see `simulator/msim_hostfs.c` for the layouts |

The Chairman's serial console reads from standard input and writes to
standard output.  Bit 1 of its flags register, which the architecture
leaves reserved, is set by `msim` once standard input has ended.  Its
timer counts one tick per instruction and reports a 1 MHz clock.

## Debugger

`msim -i` gives a prompt with these commands:

| Command | Meaning |
|---|---|
| `step [n]` | Execute `n` instructions (default 1), showing each |
| `run` | Run until a breakpoint, a watchpoint or halt.  Control-C interrupts |
| `peek addr [type]` | Show memory.  `type` is `word` (default), `half`, `byte`, `instr` or `string` |
| `poke addr value [type]` | Write memory |
| `show reg [type]` | Show a register: `r0` to `r15`, `sp`, `lr`, `ir`, `sr`, `pc`, or `ar0` and so on for the alternative bank |
| `set reg value` | Write a register |
| `dump` | Show both register banks and the flags |
| `breakpoint [addr]` | Toggle a breakpoint, or list them.  Up to 10 |
| `watchpoint add expr` | Stop when a Lua expression becomes true; `watchpoint list` and `watchpoint delete n` manage them |
| `help` | List the commands |
| `quit` | Leave |

Watchpoint expressions are Lua, evaluated after every instruction.  They
can use `r[n]` and `ar[n]` for the two register banks (with `sp`, `lr`,
`ir`, `sr` and `pc` as indices), and `word[a]`, `half[a]` and `byte[a]`
for memory:

```
watchpoint add r[1] == 0xdeadbeef and word[0x08000010] == 0
```

## Semantics worth knowing

The simulator is the reference implementation of `reference.md`.  Where
that document says UNPREDICTABLE the simulator does something particular
and unremarkable; do not rely on it.  Reserved encodings print a warning
and are skipped.  Accesses to a chip select with nothing attached print a
warning, read as zero and are otherwise ignored.
