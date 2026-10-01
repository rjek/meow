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
