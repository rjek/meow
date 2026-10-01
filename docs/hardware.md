# MEOW in hardware: toolchain, boards and approach

Research of October 2026 towards a VHDL implementation, with two
requirements: the toolchain must be free software, and the board must
be one anyone can buy for the price of a few books.

## The free toolchain

Every stage exists and is in daily use:

- **Simulation**: GHDL, or NVC, both free VHDL simulators with good
  VHDL-2008 coverage.  Either runs a testbench that reads `msim -v`'s
  trace, which prints every instruction and the registers after it,
  and compares state instruction by instruction; msim is the golden
  model and the `tests/sim` ROMs are the first test suite.
- **Synthesis**: Yosys, with VHDL through the GHDL plugin, which the
  OSS CAD Suite ships as `plugins/ghdl.so`.  The plugin is labelled
  experimental and is used widely; the synthesisable subset of
  VHDL-2008 is what it takes, which is all a CPU needs.
- **Place and route**: nextpnr.  Mature for Lattice iCE40 (IceStorm)
  and ECP5 (Trellis); supported for Gowin (Apicula) and Cologne Chip
  GateMate (Peppercorn, with the vendor's backing); experimental for
  Xilinx 7-series.
- **Programming**: openFPGALoader.

## How big MEOW is

MEOW is RV32I-class work for an FPGA: a 16-bit decode, which is less
than RISC-V's; 32 registers of 32 bits in two banks, where RV32I has
32 in one; a 32-bit adder and a barrel shifter; no multiplier; the
writeback addressing modes, which cost an adder; and the alternative
bank reads, which cost the register file a port.  The published
figures for small RISC-V cores set the scale: FemtoRV at about 1,000
LUT4s on iCE40, VexRiscv's smallest at 1,100 to 1,500, PicoRV32 at
1,000 to 2,000.  A multi-cycle MEOW core should land between 1,500
and 2,500 LUT4s, and the Chairman, the IOC with two UARTs, SPI, GPIO
and the clock, and an SDR SDRAM controller perhaps 1,500 more.  Any
part above about 8,000 LUT4s is comfortable.  The register file wants
two reads and a write a cycle, which in block RAM means two copies.

Logic is not the constraint.  Memory is: the ROM is 103 KB and
Catflap wants 256 KB to 1 MB of RAM, and that decides the part.

| Part | LUT4s | Block RAM | Where the RAM would be |
|---|---|---|---|
| Lattice iCE40 UP5K | 5,280 | 120 Kbit, plus 1 Mbit SPRAM (128 KB) | ROM fits the SPRAM; no room for the RAM, and boards have none |
| Gowin GW1NR-9 (Tang Nano 9K) | 8,640 | 468 Kbit (58 KB) | 8 MB SDRAM in the package; ROM would have to live in SDRAM too |
| Gowin GW2AR-18 (Tang Nano 20K) | 20,736 | 828 Kbit (103 KB) | 8 MB SDRAM in the package; ROM just fits block RAM |
| Lattice ECP5-25F (Colorlight i5, ULX3S-12F is the 12F) | 24,000 | 1,008 Kbit (126 KB) | 8 to 32 MB SDRAM on the board |
| Lattice ECP5-45F (Colorlight i9) | 44,000 | 1,944 Kbit (243 KB) | 8 MB SDRAM on the board; ROM and a 128 KB RAM fit on chip |
| Lattice ECP5-85F (ULX3S-85F) | 84,000 | 3,744 Kbit (468 KB) | 32 MB SDRAM; ROM and 256 KB of RAM fit on chip, no controller needed |

So there are two shapes of system: ROM in block RAM and RAM in SDRAM
through a controller of our own, which an SDR SDRAM needs about 300
LUT4s and a few pages of VHDL for; or, on an 85F, everything in block
RAM, with the SDRAM for later.  In either, the bitstream can initialise
the block RAM with the ROM image, so the first system needs no boot
loader and no flash driver; re-flashing the ROM means re-running
nextpnr, a minute or two, which is acceptable until an SPI flash
loader is worth writing.

## Boards

Prices as found in October 2026, before shipping and tax.

| Board | FPGA | Memory | Price | Open flow | Notes |
|---|---|---|---|---|---|
| Colorlight i9 | ECP5-45F | 8 MB SDRAM | $45 to 60 as a module | mature (Trellis) | A signage controller sold as a module; needs a carrier or an adapter board and a JTAG dongle, both cheap; the i5 (25F) is $15 to 45 |
| ULX3S-85F | ECP5-85F | 32 MB SDRAM | $275, €210 | mature | Open hardware; SD card, USB, HDMI, buttons on the board; the 12F is $155, €123 |
| Tang Nano 20K | GW2AR-18 | 8 MB SDRAM in package | $25 to 50 | usable, with a caveat | The cheapest board with enough memory; Apicula issue 541, open, is a placement-dependent miscompute on this very chip |
| Tang Nano 9K | GW1NR-9 | 8 MB SDRAM in package | $16 to 39 | usable | Enough logic, little block RAM |
| Tang Primer 25K | GW5A-25 | on a carrier | about $40 | in progress | NLnet funds GW5 support in nextpnr and Apicula to 2026 |
| Olimex GateMate A1-EVB | GateMate A1 | see the board | €50 | full, vendor-backed | A European part whose maker ships the free flow; worth a look once the design is stable |
| iCEBreaker | iCE40 UP5K | none | $37 to 80 | the most mature of all | Too small in memory for Catflap; fine for the core alone |
| OrangeCrab | ECP5-25F | DDR3 | about $140 | mature | DDR3 is far harder than SDR SDRAM; not worth it here |

**Recommendation.**  Develop on ECP5, where the free flow is the most
proven and the parts have the most block RAM: a Colorlight i9 module
with an adapter is the cheap way in, and a ULX3S-85F is the board
with everything on it, including the SD card slot the IOC's SPI
master is for.  Keep the Tang Nano 20K as the second target for when
Apicula's miscompute is fixed, since at $30 with 103 KB of block RAM
and 8 MB of SDRAM in the package it is the board that would let
anyone run MEOW.

## The approach

1. **The core**, in VHDL, as a multi-cycle machine: fetch, decode,
   execute, with a memory cycle where an instruction needs one, two to
   four clocks an instruction.  No pipeline, so "`pc` reads as its own
   address" and "no visible pipeline" hold by construction.  The bus is
   a synchronous request and acknowledge with byte lanes, which the
   reference will specify alongside the UNPREDICTABLE cases.
2. **The testbench**, in GHDL or NVC, driven by `msim -v` traces of
   the `tests/sim` ROMs and then of the C tests: the same ROM, the
   same registers after every instruction, or a failure that names the
   instruction.  This is what the simulator has been for.
3. **Synthesis** for the ECP5 with Yosys and nextpnr, to learn the
   size and the clock before anything else is written.
4. **The Chairman** in VHDL: chip-select decode from the top five
   address bits, the table, interrupts, the timer; block RAM for ROM
   and RAM; Catflap boots in simulation, slowly, and then on the board.
5. **The IOC**: the UART first, which is the console; then the SDRAM
   controller, which is what makes the RAM real; then SPI, GPIO and the
   clock.  Each part gets its model in msim at the same time, so the
   drivers are written against msim and proved on the board.

The whole of it is in VHDL, in `hw/`, with GHDL simulation part of
`make check` so that the core cannot drift from msim unnoticed.

## What exists: `hw/`

The approach above, begun.  `hw/rtl/` holds the VHDL, `hw/sim/` the
testbench, and `hw/Makefile` runs it: `make check` assembles the
simulator's single-CPU tests from `tests/sim/`, has `msim -T` write a
line per instruction (its address and word, then both banks'
registers after it), and runs the same ROM in GHDL, where the
testbench compares every instruction, plays the host's part (the
coprocessor port, standard input into UART 0, an SD card on the SPI
pins) and writes what the program wrote, which must match msim's
expected output too; `make synth` gives the ECP5 figures through
Yosys and the GHDL plugin.

- `meow_pkg.vhdl`: the types, the bus records of reference section
  7, and the condition codes.
- `meow_core.vhdl`: the core, a multi-cycle machine: a fetch, one
  execute cycle in which a memory instruction also makes its bus
  transaction, and one more for a memory instruction's writeback.
  r0 to r13 of both banks are a 32-word RAM read as the instruction
  arrives on the bus and written as it ends, which the ECP5 makes of
  32 distributed-RAM cells; pc and sr of each bank are flops.  The
  interrupt is taken between instructions; `WFI` is a state; negative
  `BNV`s go out of a coprocessor port, which on the board answers
  with nothing and in the testbench answers with what msim had.
- `chairman.vhdl`: reference section 5 and the bus of section 7 for
  `NCPU` masters: round-robin grant, the chip-select table from
  generics, the per-CPU masks, pending words, timers and control
  blocks, the locks and the present mask.  A write lands before the
  same cycle's tick, as msim orders them.
- `rom.vhdl` and `ram.vhdl`: block RAM, the ROM from a file of hex
  words, both answering the cycle after the request.
- `uart.vhdl`, `spi.vhdl`, `ioc.vhdl`: the IOC of reference section
  6, every part: two UARTs with 16-byte FIFOs each way, break
  detection and the testbench's backdoor into UART 0's receive side;
  the SPI master with its chip-select bit and all four modes; the
  GPIO with its edge interrupts; the clock and counter; system
  control.  Time in the IOC is counted in ticks, the same signal the
  Chairman's timers count, and a write lands before the same edge's
  tick: a byte takes 160 (divisor + 1) ticks to go, an SPI transfer
  is busy for 16 (divisor + 1), the counter counts ticks, all exactly
  as msim counts them, so that a program's view of the status bits is
  instruction-exact against the trace.
- `meow_soc.vhdl`: the system: cores, Chairman, ROM at chip select 0,
  RAM at 1, the IOC at 2.  `TICK_FROM_CORE` makes time count
  instructions (and cycles waited in `WFI`), as msim does; a board
  counts clocks.
- `sim/sd_model.vhdl`: an SD card in SPI mode at the bit level, what
  `msim_sd.c` answers, for the testbench.
- `sim/tb_soc.vhdl`: the testbench.

- `local_mem.vhdl`: local memory, a RAM per CPU at chip selects 29
  and 30, behind the Chairman, which says whose the access is.

All ten of the simulator's tests pass.  Nine match msim's trace,
2,319 instructions at 3.9 clocks each with memory that answers the
cycle after it is asked, the UART and SPI tests among them, the
card's responses arriving bit by bit on the pins at the instruction
msim had them.  The tenth, with two CPUs, cannot match a trace
instruction for instruction, since the second CPU's progress against
the first's is what differs between msim and hardware; it runs to the
halt on a two-CPU system and its output, from both CPUs' prints, must
be msim's, which it is.  For that the coprocessor port carries ir out
as well, which a real coprocessor would want too.

And Catflap boots.  `make check-catflap` runs `os/catflap.rom` under
msim with `hw/sim/catflap.in` on the console (a command or two, then
`exit`) and then under GHDL, 256 KB of RAM and 4 KB of local memory
as the operating system's tests have, and the 1,289,421 instructions
of the boot, the shell, `uname`, `ps` and the halt match one for one,
with the same console output, in four and a half minutes of
simulation at 4.6 clocks an instruction.  Three things had to be
settled for that: the testbench feeds standard input when msim would
look at it, on a status read and every 4096 ticks once the UART is
in use, rather than whenever there is room, since the moment the
console interrupt arrives decides which instruction it lands on;
msim's devices now tick in chip-select order, so a source the IOC
raises is seen by the Chairman in the same cycle as the hardware
sees it; and the testbench's system says it is msim's machine to
`BNV #0`, since that is what the trace is of.

Synthesised for the ECP5 the whole system is about 8,000 LUT4s and
1,700 flops: the core 3,600 and 260 (its register file 32
distributed-RAM cells), the Chairman 700 and 190 for one CPU, the
IOC about 3,300 and 1,200, and the 64 KB RAM 32 block RAMs.  The
first cut of the core, with the register file as flops and a read
mux for every use, was 25,000 LUT4s on its own, which says where the
cost of a design like this goes.  The IOC is fat for what it is: its
FIFOs are flops and its counters are 32 bits wide, and both can be
cut.  The ROM is empty at synthesis, so its blocks do not show.
Nothing has been timed yet.

`make check-catflap2` boots Catflap on two CPUs the same way, on the
output alone.

Not yet done: a board's top level with a clock and reset, and timing.

## Sources

- nextpnr's supported families: https://github.com/YosysHQ/nextpnr
- Gowin support: https://github.com/YosysHQ/apicula and its wiki,
  https://github.com/YosysHQ/apicula/wiki/Nextpnr%E2%80%90Himbaechel-Gowin;
  the open miscompute: https://github.com/YosysHQ/apicula/issues/541;
  GW5 funding: https://nlnet.nl/project/nextpnr-GW-5/
- GateMate's flow: https://colognechip.com/programmable-logic/gatemate/toolchain/
  and the Olimex board: https://www.olimex.com/Products/FPGA/GateMate/
- The GHDL plugin in the OSS CAD Suite:
  https://acidbourbon.wordpress.com/2024/12/02/yosyshq-fpga-toolchain-with-vhdl-support/
- RISC-V core sizes on iCE40: https://mrwski.eu/projects/data/riscv_ice40.pdf,
  https://github.com/BrunoLevy/learn-fpga/blob/master/FemtoRV/README.md,
  https://github.com/YosysHQ/picorv32
- ECP5 block RAM: the ECP5 family data sheet,
  https://www.latticesemi.com/-/media/LatticeSemi/Documents/DataSheets/ECP5/FPGA-DS-02012-3-4-ECP5-ECP5G-Family-Data-Sheet.ashx?document_id=50461
- Boards: Tang Nano 20K, https://hackaday.com/2026/09/28/the-fpga-chronicles-exploring-the-tang-nano-20k/
  and https://www.cnx-software.com/2023/05/22/25-sipeed-tang-nano-20k-fpga-board-can-simulate-a-risc-v-core-run-linux-retro-games/;
  Tang Nano 9K, https://wiki.sipeed.com/hardware/en/tang/Tang-Nano-9K/Nano-9K.html;
  Tang Primer 25K, https://www.cnx-software.com/2024/01/03/sipeed-tang-primer-25k-board-23040-logic-cells-fpga-prototyping-development/;
  ULX3S, https://www.crowdsupply.com/radiona/ulx3s and https://www.envox.eu/product/ulx3s/;
  Colorlight i5 and i9, https://tomverbeure.github.io/2021/01/22/The-Colorlight-i5-as-FPGA-development-board.html
  and https://github.com/wuxx/Colorlight-FPGA-Projects/blob/master/colorlight_i9_v7.2.md;
  iCEBreaker, https://www.crowdsupply.com/1bitsquared/icebreaker-fpga
