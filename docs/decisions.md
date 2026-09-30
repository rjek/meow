# Design decisions

Where the original text notes, the LaTeX manual, the Lua assembler and the
simulator disagreed, these are the resolutions.  The reference manual is
written to these; the simulator and assembler are corrected to match.

1. **MOV swap bits.**  Bit 7 is byte swap, bit 6 is halfword swap, as the
   text notes and the Lua assembler had it.  The simulator had them reversed.
   Byte swap alone is ARM `REV16`, both together is `REV`, halfword swap
   alone is `ROR #16`.

2. **Shift direction bit.**  1 means left.  Only the LaTeX manual said
   otherwise.

3. **ADD/SUB three-operand form.**  An immediate of zero means `Rd += Rs`;
   any other value means `Rd = Rs + imm`.  These are two syntaxes,
   `ADD Rd, Rs` and `ADD Rd, Rs, #1..15`.  The immediate is unsigned.  The
   assembler rejects `ADD Rd, Rs, #0`.

4. **Decrease-before writeback.**  MEM writeback mode 01 decrements the
   address register by the access size before the access, as the text notes
   specify.  Neither old tool implemented it.  It makes a full-descending
   push one instruction: `STR r0, [sp, #-4]!`.

5. **CMP flags.**  CMP is ARM `SUBS` with the result discarded: N and Z from
   the result, C set when `Rn >= operand` unsigned (no borrow), V set on
   signed overflow.  The text notes' own condition table (CS = HS, CC = LO,
   GE is N = V) only works with these semantics; its prose formula for C
   contradicted it.  The simulator set C as a borrow and never set V.  TST
   sets N from bit 31 and Z from the result, and leaves C and V alone.

6. **LS condition.**  C = 0 or Z = 1.  The simulator had "and".

7. **Sub-word loads.**  `LDRB` zero-extends into the full register.  The
   low-halfword load (`LDRH`) zero-extends.  The high-halfword load
   (`LDRHH`) writes bits 31:16 and leaves 15:0 intact.  The old tools merged
   every sub-word load, which costs a C compiler a clearing instruction on
   every `unsigned char` and `unsigned short` load.  A word can still be
   built from two halfword loads, in the order low then high.  Stores are
   unaffected.

8. **Arithmetic shifts and rotates.**  `ASR` has encodings of its own, with
   bit 12 set and bits 7:5 clear; the logical shifts and rotates leave bit
   12 clear (see 11 for what took the rest of that space).  `ASL` is an
   assembler alias for `LSL`.  A shift or rotate amount taken from a
   register uses its low five bits only.

9. **Reset state.**  The CPU resets into the normal bank with every register
   in both banks zero, and the Chairman has all interrupts masked.  The I
   bit of the status register is read-only and reads 1 only in the
   interrupt bank.  The text notes said reset entered interrupt mode; the
   only existing code (Catflap) and the simulator assumed the normal bank,
   and a machine with no privilege levels gains nothing from the other
   choice.

10. **Corrections.**  Branch range is -512 to +510 bytes.  N means negative.
    The address space is 32 chip selects of 128 MB (a five-bit selector).
    The serial console occupies 0x2410 to 0x241b of the system controller.
    BNV operands are even.  Unaligned accesses, and writeback onto the value
    register of the same MEM instruction, are UNPREDICTABLE.  Reading `pc`
    gives the address of the current instruction.

## Instruction set additions

Both were prototyped end to end (ISA description, simulator, assembler,
disassembler, compiler) and measured on `make bench`, each against the
same build with only that change switched off.

11. **Stack-relative LDR and STR: kept.**  `LDR|STR Rv, [sp, #n]`, a word
    at an offset of 0 to 124, in the `1011 vvvv 01Lo oooo` corner of the
    shift opcode.  The compiler has no other way to reach a stack slot
    than `ADD ir, sp, #n` or `MOV ir, sp; ADD ir, #n` before the access,
    so every spill and every local without a register cost two or three
    instructions.  Measured: 6.7% fewer instructions over the suite and
    12.8% on softfp, whose 64-bit values live on the stack, for 2.1% less
    code.  To make room `ASR` moved to encodings of its own with bit 12
    set, since the old shift encoding left that bit free with left shifts
    and rotates and would have overlapped.

12. **ADDS and SUBS: tried and dropped.**  Add and subtract setting the
    flags, `Rd, #0..31` and `Rd, Rs`, in the remaining `1011 rrrr 1xxx
    xxxx` space.  The compiler fused them into `n-- > 0` loops, loops
    that only count, 64-bit adds and the C carry idiom `x += y; if (x <
    y)`, remapping the branch condition when the compare it replaced was
    not against the same value.  Measured: 0.7% fewer instructions over
    the suite, 8.6% on crc and 6.4% on bits, nothing elsewhere.  Nearly
    every loop compares its counter against a bound in a register and
    keeps its `CMP`, and the branch condition remapping leant on the
    compare being the last thing in its block.  Not worth four encodings
    and that fragility; the space is reserved again.

13. **Division by a constant: the wrong shape.**  Not an instruction, but
    the same lesson.  Multiplying by a reciprocal through `__umulhi`
    measured slower than `__udiv` for every dividend size, twice as slow
    for large ones and nine times for small: the subtract loop costs
    about four instructions per quotient bit, the shift-and-add multiply
    about seven per bit of a full 32-bit constant.  Without a multiplier
    the library divide stays; the compiler keeps the code switched off.

## The compiler

The C compiler's choices are in `compiler.md` with the measurements that
made them; 11 to 13 above are the ones that touched the instruction set.

## The C library

14. **PDCLib and musl's maths.**  With the compiler at C90, PDPCLIB was the
    only whole library that would compile; once the C99 to C23 front end
    was merged, PDCLib (CC0, portable, a dozen platform functions) became
    the right one, with musl's `src/math` (MIT) for the maths PDCLib
    lacks and as little else from musl as possible, its lowest layer
    being Linux system calls.  The platform layer is a console and a
    heap; there is no filesystem or clock to pretend to.  The imports
    are verbatim bar the small patches listed in `libc.md`, all for
    upstream bugs; the two that worked round compiler limitations went
    once the C99 front end fixed them.  `mld` grew `ar` archive support
    rather than the project growing an archiver: the host's `ar` writes
    the format, and a library is no use if every program links all of
    it.

15. **`strtod` from musl.**  PDCLib's `strtod` turned out to be a
    placeholder: inexact in decimal and an endless loop in hexadecimal.
    The choices were to repair it, to write one on PDCLib's big
    integers, which its exact `printf` already has in the ROM, or to
    take musl's scanner.  musl's won: reading a decimal numeral to the
    nearest double is easy to get subtly wrong, musl's has been right
    for a decade, and it came unchanged but for one digit of guard that
    a 53-bit `long double` needs (`libc.md`).  It costs 3.7 KB of ROM
    over the placeholder.  Its `FILE` plumbing stayed behind; a
    fourteen-line header makes it read a string.

## Lua

16. **Lua as the acceptance test.**  A 370 KB interpreter that leans on
    setjmp, 64-bit integers, doubles, varargs, unions and the whole of
    stdio is a better test of a toolchain than anything written for the
    purpose.  It compiled unchanged.  Getting it to run found a register
    allocation bug (a block copy's operand could be allocated `at`, the
    register the copy counts through), a typing bug in the middle end
    (unsigned 64-bit results treated as signed), and in PDCLib a
    misspelt `remquo`, empty comparison macros, five faults in `%g`,
    four in `%a` and the `strtod` of 15.
    The runtime grew a real `argv`, `setjmp`, a clock from the host, and
    a stack placed from the RAM size the Chairman reports, so `msim -m`
    can give a program as much memory as it needs.  None of that is Lua
    specific, and the sources are stock so that a new Lua drops in.

## Possible future expansions

Recorded so that the measurements are not lost; none is decided.

17. **A PC-relative load.**  Every constant or address the compiler
    cannot make with `LDI` comes from a literal pool as `LDI #off ;
    ADD ir, pc ; LDR rd, [ir]`, three instructions where ARM spends one.
    When this was first measured there were about 10,000 of these
    across the kernel, the C library, Lua and the programs, worth some
    40 KB of a 470 KB ROM, and a single `LDR rd, [pc, #n]` looked like
    the one instruction to add.  Most of them, 8,600, were calls
    fetching the callee's address.  The compiler now keeps that address
    in the word after the call and loads `pc` through `lr` (`abi.md`),
    which needed no new instruction, and 1,100 pool loads are left:
    about 4 KB.  The reserved `1011 rrrr 1xxx xxxx` space still has
    room for a word-scaled seven-bit forward offset, reaching 508
    bytes, but it is no longer the first thing to spend it on;
    `rom-size.md` has the list in order.  Deferred, like the rest of
    that list: the instruction set is not to change for the operating
    system's sake until the system has been run and measured as it is.

## Toolchain

- **Binary formats.**  Flat binary and ELF32 little-endian with a private
  machine number.  Branches resolve within a section at assembly time;
  cross-section and cross-file references go through literal pools or
  `DCD`, so the relocation types are `ABS32`, `ABS16` and `ABS8`.  No
  libelf: the reader and writer are our own.
- **Single source of truth.**  `isa/meow.isa` describes every encoding.  A
  generator produces the C tables used by the assembler, simulator and
  disassembler, and the encoding diagrams in the documentation.
- **Bit-immediate syntax.**  `AND`, `ORR`, `EOR`, `BIC`, `TST` and friends
  take a value with exactly one bit set, `ORR r0, #0x20`, as on ARM.  The
  Lua assembler took a bit number, `BIS r0, #5`.  This is the one deliberate
  break with the old syntax.
