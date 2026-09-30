# MEOW Architecture Reference

MEOW is a 32-bit RISC microcontroller with 16-bit instructions.  It has
sixteen 32-bit registers in each of two banks, eight opcodes, a
condition-code register, and a simple memory map with a system controller.
It is designed to fit in a small FPGA and to be comfortable for anyone who
has written ARM assembler.  The name is a disingenuous acronym for
Microprocessor With Eight Opcodes.

This document describes MEOW version 0 as implemented by the `msim`
simulator and targeted by the `mas` assembler.  Where earlier notes,
tools and documents disagreed, `decisions.md` records what was chosen and
why.

Contents:

1. Programmer's model
2. Instruction set
3. BNV extension space
4. Memory map
5. Chairman system controller

Notation: `Rd`, `Rs`, `Rn`, `Rm`, `Rv` and `Ra` are registers; `{A}Rn` is a
register that may be in the alternative bank; `#imm` is an immediate.
Encoding diagrams show bit 15 on the left.  The encoding diagrams and
field tables are generated from `isa/meow.isa`, which is the single
source of truth for the bit layouts.

## 1. Programmer's model

### 1.1 Registers

Each bank has sixteen 32-bit registers, `r0` to `r15`.  Instructions
identify a register with four bits.  Five registers have architectural or
conventional roles:

| Register | Alias | Role |
|---|---|---|
| r0 to r10 | | General purpose |
| r11 | sp | Stack pointer by convention; general purpose to the hardware |
| r12 | lr | Link register by convention; general purpose to the hardware |
| r13 | ir | Immediate register: the target of LDI; otherwise general purpose |
| r14 | sr | Status register |
| r15 | pc | Program counter |

The only registers the hardware treats specially are `ir` (LDI writes it),
`sr` (the flags live in it) and `pc`.  Everything else is convention; the
C ABI in `abi.md` assigns further names (`a1` to `a4`, `v1` to `v6`, `at`).

### 1.2 Register banks

There are two complete banks of sixteen registers.  One bank is active;
the other is the *alternative* bank.  Taking an interrupt swaps the banks
so that the interrupt handler has its own stack pointer, link register and
working registers, and returns with `IRQRTN` which swaps them back.

Most instructions operate on the active bank only.  `MOV`, `CMP` and `TST`
can name registers in the alternative bank; in assembler these are written
with an `a` prefix: `ar0` to `ar15`, `asp`, `alr`, `air`, `asr`, `apc`.
Writing `apc` changes where the other bank resumes.

### 1.3 Status register

```
 31 30 29 28 27 .............................. 1  0
+--+--+--+--+----------------------------------+--+
| N| Z| C| V|             reserved             | I|
+--+--+--+--+----------------------------------+--+
```

| Bit | Name | Meaning |
|---|---|---|
| 31 | N | Negative: bit 31 of the last CMP or TST result |
| 30 | Z | Zero: the last CMP or TST result was zero |
| 29 | C | Carry: the last CMP did not borrow (Rn was greater than or equal to the operand, unsigned) |
| 28 | V | Overflow: the last CMP overflowed as a signed subtraction |
| 27:1 | | Reserved.  Read as zero; write what was read |
| 0 | I | Interrupt mode.  Read-only; 1 in the interrupt bank's status register |

Only `CMP` and `TST` write the flags.  Arithmetic and logic instructions
never do, so a loop counter needs an explicit `CMP`.  The flags follow ARM
conventions exactly, which is what makes the condition codes in the table
under `B` mean what an ARM programmer expects.

`sr` can be read and written like any register.  Writing N, Z, C and V has
effect; writing I does not.

### 1.4 Program counter

`pc` holds the address of the instruction being executed.  Instructions are
two bytes and must be halfword aligned, so bit 0 is always zero; setting it
is UNPREDICTABLE.  MEOW has no visible pipeline: an instruction that reads
`pc` sees its own address, with no offset.

After an instruction completes, `pc` advances by two unless the
instruction wrote `pc` in the active bank, in which case execution
continues at the written address.  This applies to every instruction that
can name `pc` as a destination: `ADD pc, ir` is a computed jump, `MOV pc,
lr` is a return, and `LDR pc, [sp], #4` pops a return address.

### 1.5 Reset

On reset every register in both banks is zero, the normal bank is active,
the I bit reads 0, and the Chairman has all interrupts masked.  Execution
begins at address 0, which is normally ROM.

### 1.6 Interrupts

When the Chairman asserts an interrupt and the CPU is not already in
interrupt mode, the banks swap and `pc` in the (now active) interrupt bank
is set to 32.  The interrupt bank's status register has I set.  Interrupts
arriving while in interrupt mode are held pending by the Chairman.

The handler returns with `IRQRTN` (`BNV #4`), which swaps the banks back
and continues at the interrupted instruction's successor; the interrupted
bank's `pc` was never disturbed.  A handler must clear the source of the
interrupt in the Chairman before returning, or it will be taken again.

Because the interrupt bank's registers persist between interrupts, a
handler can keep state in them (a stack pointer, for instance) across
invocations.

### 1.7 Memory

Memory is byte addressed and little-endian.  Loads and stores are 8, 16 or
32 bits wide and must be aligned to their size; unaligned accesses are
UNPREDICTABLE.  Instruction fetches are 16 bits wide.

The address space is split into 32 chip selects of 128 MB each, chosen by
the top five bits of the address (see section 4).

## 2. Instruction set

Every instruction is 16 bits.  Bits 15:13 select one of the eight opcodes;
bit 12 usually selects a sub-form; bits 11:8 usually hold the destination
register.  All immediates are unsigned unless stated otherwise.

Summary:

| Opcode | Mnemonics | Purpose |
|---|---|---|
| 000 | B, BL, BNV | Conditional branch; extension space |
| 001 | ADD | Addition |
| 010 | SUB | Subtraction |
| 011 | CMP, TST | Compare and test, setting flags |
| 100 | MOV, LDI | Register move with swaps; load immediate |
| 101 | LSL, LSR, ASR, ROL, ROR, LDR, STR | Shifts and rotates; stack words |
| 110 | MVN, AND, ORR, EOR, BIC, ORN, EON | Bitwise operations |
| 111 | LDR, STR | Memory access |

### 2.1 B: conditional branch

<!-- isa:B -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 0| 0|   cond    |           off            |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 12:9 | cond | Condition code |
| 8:0 | off | Signed offset from this instruction, in halfwords |

Syntax:

```
B{cond} label
BNV #imm
```
<!-- /isa -->

If the condition holds, `pc` becomes `pc + 2 * offset`, where the offset is
a signed 9-bit number of halfwords: a range of -512 to +510 bytes from the
branch itself.  Otherwise the branch is a no-op.  The assembler computes
the offset from a label and rejects targets that are out of range, or
expands them, as described in `assembler.md`.

Conditions:

| Code | Suffix | Meaning | Flags |
|---|---|---|---|
| 0000 | EQ | Equal | Z = 1 |
| 0001 | NE | Not equal | Z = 0 |
| 0010 | CS, HS | Carry set, unsigned higher or same | C = 1 |
| 0011 | CC, LO | Carry clear, unsigned lower | C = 0 |
| 0100 | MI | Negative | N = 1 |
| 0101 | PL | Positive or zero | N = 0 |
| 0110 | VS | Overflow | V = 1 |
| 0111 | VC | No overflow | V = 0 |
| 1000 | HI | Unsigned higher | C = 1 and Z = 0 |
| 1001 | LS | Unsigned lower or same | C = 0 or Z = 1 |
| 1010 | GE | Signed greater than or equal | N = V |
| 1011 | LT | Signed less than | N != V |
| 1100 | GT | Signed greater than | N = V and Z = 0 |
| 1101 | LE | Signed less than or equal | N != V or Z = 1 |
| 1110 | AL | Always (the default, written as plain B) | |
| 1111 | NV | Never: the BNV extension space, see section 3 | |

`B` is the only conditional instruction.  Conditional execution of
anything else is done by branching around it.

`BL` is not an instruction.  The assembler expands `BL target` to
`ADD lr, pc, #4` followed by `B target`, so that `lr` holds the address of
the instruction after the branch.  With a condition suffix the `ADD` is
still unconditional; `lr` is set whether or not the call is taken.

### 2.2 ADD: addition

<!-- isa:ADD3 -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 0| 1| 0|    rd     |    imm    |    rs     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Destination register |
| 7:4 | imm | Immediate; zero selects Rd += Rs |
| 3:0 | rs | Source register |

Syntax:

```
ADD Rd, Rs
ADD Rd, Rs, #imm
```
<!-- /isa -->

The three-operand form has two meanings, selected by the immediate:

| Immediate | Syntax | Operation |
|---|---|---|
| 0 | `ADD Rd, Rs` | Rd = Rd + Rs |
| 1 to 15 | `ADD Rd, Rs, #imm` | Rd = Rs + imm |

There is no way to write `Rd = Rs + 0`; use `MOV`.  The assembler rejects
`ADD Rd, Rs, #0`.

<!-- isa:ADD8 -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 0| 1| 1|    rd     |          imm          |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Destination register |
| 7:0 | imm | Unsigned immediate added to Rd |

Syntax:

```
ADD Rd, #imm
```
<!-- /isa -->

`ADD Rd, #imm` adds an 8-bit unsigned immediate (0 to 255) to `Rd`.

Neither form sets the flags.  Adding to `pc` is a relative jump; `ADD pc,
ir` after an `LDI` is the standard long jump.

### 2.3 SUB: subtraction

<!-- isa:SUB3 -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 1| 0| 0|    rd     |    imm    |    rs     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Destination register |
| 7:4 | imm | Immediate; zero selects Rd -= Rs |
| 3:0 | rs | Source register |

Syntax:

```
SUB Rd, Rs
SUB Rd, Rs, #imm
```
<!-- /isa -->

| Immediate | Syntax | Operation |
|---|---|---|
| 0 | `SUB Rd, Rs` | Rd = Rd - Rs |
| 1 to 15 | `SUB Rd, Rs, #imm` | Rd = Rs - imm |

<!-- isa:SUB8 -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 1| 0| 1|    rd     |          imm          |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Destination register |
| 7:0 | imm | Unsigned immediate subtracted from Rd |

Syntax:

```
SUB Rd, #imm
```
<!-- /isa -->

`SUB Rd, #imm` subtracts an 8-bit unsigned immediate from `Rd`.  Neither
form sets the flags.

### 2.4 CMP: compare

<!-- isa:CMPI -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 1| 1| 0|    rn     |          imm          |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rn | Register compared |
| 7:0 | imm | Signed immediate compared against |

Syntax:

```
CMP Rn, #imm
```
<!-- /isa -->

<!-- isa:CMPR -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 1| 1| 1|    rn     | 0| 0| w| x|    rm     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rn | First register |
| 5 (w) | bn | 1 if Rn is in the alternative bank |
| 4 (x) | bm | 1 if Rm is in the alternative bank |
| 3:0 | rm | Second register |

Syntax:

```
CMP {A}Rn, {A}Rm
```
<!-- /isa -->

`CMP` subtracts the second operand from the first, discards the result and
sets N, Z, C and V exactly as ARM's `SUBS` would:

```
result = Rn - operand
N = result[31]
Z = (result == 0)
C = (Rn >= operand)          unsigned comparison: 1 means no borrow
V = signed overflow of the subtraction
```

The immediate form takes a signed 8-bit value, -128 to 127.  The register
form can take either register from either bank, which is how an interrupt
handler inspects the interrupted bank.  Bit 6 of the register form is
reserved and must be zero.

### 2.5 TST: test a bit

<!-- isa:TST -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 0| 1| 1| 1|    rn     | 1| 0| w|     bit      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rn | Register tested |
| 5 (w) | bn | 1 if Rn is in the alternative bank |
| 4:0 | bit | Bit number tested |

Syntax:

```
TST {A}Rn, #value
```
<!-- /isa -->

`TST` ANDs the register with `1 << bit` and sets N and Z from the result.
C and V are unchanged.  The assembler takes the mask value, `TST r0,
#0x100`, and encodes the bit number; the value must have exactly one bit
set.  Bit 6 is reserved and must be zero.

### 2.6 MOV: move between registers

<!-- isa:MOV -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 0| 0|    rd     | b| h| w| x|    rs     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Destination register |
| 7 (b) | bsw | Swap each byte with its neighbour (REV16) |
| 6 (h) | hsw | Swap the two halfwords |
| 5 (w) | bd | 1 if Rd is in the alternative bank |
| 4 (x) | bs | 1 if Rs is in the alternative bank |
| 3:0 | rs | Source register |

Syntax:

```
MOV{B}{W} {A}Rd, {A}Rs
```
<!-- /isa -->

Copies `Rs` to `Rd`, either of which may be in the alternative bank.  On
the way the value may be byte-swapped within each halfword (the `B` bit,
equivalent to ARM `REV16`), halfword-swapped (the `W` bit, equivalent to
`ROR #16`), or both (`MOVBW`, a full endian reversal like ARM `REV`).

`MOV pc, Rs` is a jump.  `MOV r0, r0` is the conventional `NOP`.

The assembler also accepts `MOV Rd, #imm` for any 32-bit constant and
synthesises it from `LDI`, `MOV`, `LSL` and `ADD` (see `assembler.md`);
that form uses `ir` as scratch.

### 2.7 LDI: load immediate

<!-- isa:LDI -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 0| 1|                imm                |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:0 | imm | Signed immediate loaded into ir |

Syntax:

```
LDI #imm
```
<!-- /isa -->

Loads a sign-extended 12-bit immediate (-2048 to 2047) into `ir`.  This is
the only way to get a constant larger than 8 bits into a register in one
instruction, which is why `ir` exists.  Larger constants are built by
shifting and adding, or loaded from memory.

### 2.8 Shifts and rotates

<!-- isa:SHI -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 1| 0|    rd     | d| R| 0|     imm      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Register shifted |
| 7 (d) | left | 1 shifts left, 0 shifts right |
| 6 (R) | rot | 1 rotates, 0 shifts |
| 4:0 | imm | Shift amount |

Syntax:

```
LSL|LSR|ROL|ROR Rd, #imm
```
<!-- /isa -->

<!-- isa:SHR -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 1| 0|    rd     | d| R| 1| 0|    rs     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Register shifted |
| 7 (d) | left | 1 shifts left, 0 shifts right |
| 6 (R) | rot | 1 rotates, 0 shifts |
| 3:0 | rs | Register holding the shift amount (low five bits used) |

Syntax:

```
LSL|LSR|ROL|ROR Rd, Rs
```
<!-- /isa -->

<!-- isa:ASRI -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 1| 1|    rd     | 0| 0| 0|     imm      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Register shifted |
| 4:0 | imm | Shift amount |

Syntax:

```
ASR Rd, #imm
```
<!-- /isa -->

<!-- isa:ASRR -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 1| 1|    rd     | 0| 0| 1| 0|    rs     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rd | Register shifted |
| 3:0 | rs | Register holding the shift amount (low five bits used) |

Syntax:

```
ASR Rd, Rs
```
<!-- /isa -->

The two option bits of the logical form select the operation:

| D | R | Mnemonic | Operation |
|---|---|---|---|
| 1 | 0 | LSL | Logical shift left, zeros in |
| 0 | 0 | LSR | Logical shift right, zeros in |
| 1 | 1 | ROL | Rotate left |
| 0 | 1 | ROR | Rotate right |

`ASR`, the arithmetic shift right, has its own encodings with bit 12 set.
The rest of that space holds the stack-relative `LDR` and `STR` (section
2.10); the encodings `1011 rrrr 1xxx xxxx` are reserved.

`ASL` is accepted by the assembler as another name for `LSL`.  The shift
amount is 0 to 31, either a 5-bit immediate or the low five bits of `Rs`;
higher bits of `Rs` are ignored.  A shift by zero leaves the register
unchanged.  Flags are not affected.

### 2.9 Bitwise operations

<!-- isa:BITR -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 1| 0| n|    rd     | op  | 0| 0|    rs     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 12 (n) | inv | Invert the operand before the operation |
| 11:8 | rd | Destination register |
| 7:6 | op | 0 NOT, 1 AND, 2 ORR, 3 EOR |
| 3:0 | rs | Operand register |

Syntax:

```
MVN|AND|ORR|EOR|BIC|ORN|EON Rd, Rs
```
<!-- /isa -->

<!-- isa:BITI -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 1| 0| n|    rd     | op  | 1|     bit      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 12 (n) | inv | Invert the operand before the operation |
| 11:8 | rd | Destination register |
| 7:6 | op | 0 NOT, 1 AND, 2 ORR, 3 EOR |
| 4:0 | bit | Operand is 1 shifted left by this |

Syntax:

```
MVN|AND|ORR|EOR|BIC|ORN|EON Rd, #value
```
<!-- /isa -->

The operand is either a register or a single bit, `1 << bit`.  If the N
bit is set the operand is inverted before use.  The operation is then:

| op | N = 0 | N = 1 | Operation |
|---|---|---|---|
| 00 | MVN | reserved | Rd = NOT operand |
| 01 | AND | BIC | Rd = Rd AND operand |
| 10 | ORR | ORN | Rd = Rd OR operand |
| 11 | EOR | EON | Rd = Rd EOR operand |

`MVN Rd, Rs` writes the complement of `Rs`; `NOT Rd` is the assembler's
name for `MVN Rd, Rd`.  `BIC Rd, #0x80` clears bit 7; `ORR Rd, #0x80` sets
it.  Zeroing a register is `EOR Rd, Rd`, which the assembler emits for
`MOV Rd, #0`.

In assembler the immediate is written as the mask value, as on ARM, and
must have exactly one bit set: `AND r0, #0x10`.  Masks with several bits
set are synthesised through `ir` by the assembler.  Flags are not affected.

### 2.10 LDR and STR: memory access

<!-- isa:MEM -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 1| 1| L|    rv     | S| H| W| D|    ra     |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 12 (L) | store | 1 stores, 0 loads |
| 11:8 | rv | Value register |
| 7 (S) | half | 1 for a halfword access |
| 6 (H) | hilo | Halfword: 1 low half, 0 high half.  Otherwise: 1 word, 0 byte |
| 5 (W) | wb | Writeback; with D: 00 none, 01 decrease before |
| 4 (D) | dir | Writeback direction: 10 decrease after, 11 increase after |
| 3:0 | ra | Address register |

Syntax:

```
LDR|STR{B|H|HH} Rv, [Ra]
LDR|STR{B|H|HH} Rv, [Ra], #size
LDR|STR{B|H|HH} Rv, [Ra], #-size
LDR|STR{B|H|HH} Rv, [Ra, #-size]!
```
<!-- /isa -->

`Rv` is the register loaded or stored and `Ra` holds the address.  There is
no offset field: the address is exactly `Ra`, optionally adjusted by the
access size before or after the transfer.

| S | H | Suffix | Access |
|---|---|---|---|
| 0 | 0 | B | Byte.  A load zero-extends into all 32 bits |
| 0 | 1 | (none) | Word |
| 1 | 1 | H | Halfword, low.  A load zero-extends into all 32 bits; a store writes bits 15:0 |
| 1 | 0 | HH | Halfword, high.  A load writes bits 31:16 and leaves 15:0 untouched; a store writes bits 31:16 |

| W | D | Syntax | Writeback |
|---|---|---|---|
| 0 | 0 | `[Ra]` | None |
| 0 | 1 | `[Ra, #-n]!` | Ra decremented by the access size before the access |
| 1 | 0 | `[Ra], #-n` | Ra decremented by the access size after the access |
| 1 | 1 | `[Ra], #n` | Ra incremented by the access size after the access |

`n` must equal the access size (1, 2 or 4).  With a full-descending stack
in `sp`, `STR r0, [sp, #-4]!` pushes and `LDR r0, [sp], #4` pops.  The
`LDRH` then `LDRHH` pair through a post-incrementing pointer assembles a
word from two halfwords.

Using the same register as `Rv` and `Ra` with writeback enabled is
UNPREDICTABLE.  Loading `pc` continues execution at the loaded address.

A word on the stack has its own form with an offset, the one case where
the address is not a register:

<!-- isa:SPMEM -->
```
 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
| 1| 0| 1| 1|    rv     | 0| 1| L|     imm      |
+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
```

| Bits | Field | Meaning |
|---|---|---|
| 11:8 | rv | Value register |
| 5 (L) | store | 1 stores, 0 loads |
| 4:0 | imm | Word offset from sp |

Syntax:

```
LDR|STR Rv, [sp, #imm]
```
<!-- /isa -->

The offset is a word count, so `LDR Rv, [sp, #n]` reaches the 32 words
from `sp` to `sp + 124`; `n` must be a multiple of 4.  Only words: a
byte or halfword on the stack is addressed through a register as usual.

## 3. BNV extension space

A branch with the NV condition would never be taken, so the encoding
`0001 111o oooo oooo` is used for operations outside the eight opcodes.
The 9-bit field is read exactly as a branch offset: a signed number of
halfwords, so operands are even numbers from -512 to +510, written `BNV
#n`.

Non-negative operands are defined by this specification and every
implementation provides them; an implementation that does not support one
treats it as a no-op.  Negative operands are implementation defined.

| Operand | Alias | Effect |
|---|---|---|
| 0 | | `ir` = CPU model: byte 0 implementor (0 = MEOW project), byte 1 model (0 = msim, 1 = MEOW1), byte 2 revision, byte 3 instruction set version (0) |
| 2 | | `ir` = bus ID of this CPU in bits 4:0 |
| 4 | IRQRTN | Return from interrupt: swap banks.  No-op when not in interrupt mode |

The simulator's negative operands are listed in `simulator.md`.

## 4. Memory map

```
 31       27 26                                             0
+-----------+------------------------------------------------+
|   chip    |                     offset                     |
|  select   |                    (128 MB)                    |
+-----------+------------------------------------------------+
```

| Chip select | Base address | Device |
|---|---|---|
| 0 | 0x00000000 | ROM.  Reset vector at 0, interrupt vector at 32 |
| 1 | 0x08000000 | RAM |
| 2 to 30 | | Other devices |
| 31 | 0xf8000000 | Chairman system controller |

An access to a chip select with nothing attached reads as zero and writes
are ignored (the simulator warns).  Device identities can be discovered
through the Chairman's chip-select table rather than assumed, except for
the three above, which are fixed.

## 5. Chairman system controller

Chairman lives at chip select 31.  All its registers are 32 bits wide and
must be accessed with word loads and stores at word-aligned addresses.
Offsets below are from 0xf8000000.

| Offset | Access | Register |
|---|---|---|
| 0x0000 to 0x1fff | R | Chip-select table: 32 entries of 256 bytes.  The first word of an entry is the device ID, the second the device's size in bytes, or 0 where that means nothing |
| 0x2000 to 0x207f | RW | Interrupt masks: one word per CPU, 32 CPUs |
| 0x2400 | RW | Pending interrupts |
| 0x2404 | R | Timer clock frequency in Hz |
| 0x2408 | RW | Timer reload value |
| 0x240c | RW | Timer current value |
| 0x2410 | R | Serial console flags |
| 0x2414 | R | Serial console input byte |
| 0x2418 | W | Serial console output byte |

### 5.1 Chip-select table

Entry `n` describes chip select `n`.  Its first word identifies the device:
bits 31:16 are the vendor (0 is the MEOW project) and bits 15:0 the
vendor's device number.  A value of 0xffffffff means nothing is attached.
The remaining words of an entry are reserved.

| Vendor 0 device | Meaning |
|---|---|
| 0 | ROM |
| 1 | RAM |
| 2 | Chairman, this specification |
| 3 | IOC, section 6 |

### 5.2 Interrupts

Chairman has 32 interrupt sources; the timer is source 31.  The pending
register has one bit per source, set when the source raises its interrupt.
Writing the pending register clears the bits that are set in the written
word (`pending &= ~written`).

Each CPU has a mask word at `0x2000 + 4 * cpu`.  A 1 bit means the CPU
wants that source.  Whenever `pending & mask` is non-zero the CPU is
interrupted, as described in section 1.6; a handler clears the pending
bit before returning.  All masks are zero at reset.

### 5.3 Timer

Writing a non-zero reload value starts the timer: it counts down from the
reload value once per clock and, on reaching zero, raises interrupt 31 and
reloads.  A reload value of zero (the reset state) stops it.  Writing the
current value register sets the count directly.  The frequency register is
read-only and reports the clock the timer counts at; the simulator ticks
once per instruction and reports 1 MHz.

### 5.4 Serial console

The flags register has bit 0 set when a fresh byte is waiting.  Reading the
input register returns the most recent byte and clears the fresh bit.
Writing a byte to the output register sends it.  There is no interrupt for
the console; poll the flags.

## 6. IOC input/output controller

The IOC is what a MEOW system has besides memory and the Chairman: the
serial ports, a bus for storage, some pins and a clock.  It is vendor 0
device 3 in the chip-select table, at whichever chip select the table
says; a system need not have one, and one that has one need not have
every part of it, since the identification register says what is
there.  As with the Chairman, every register is 32 bits wide and is
accessed with word loads and stores at word-aligned addresses; bits
described as reserved read as zero and ignore writes.  Offsets below are
from the IOC's chip select base.  The parts are 256 bytes apart so that
a second instance of any of them can follow the first.

| Offset | Part |
|---|---|
| 0x0000 | Identification and clock |
| 0x0100 | UART 0, the console |
| 0x0200 | UART 1 |
| 0x0300 | SPI master |
| 0x0400 | GPIO |
| 0x0500 | Real-time clock and counter |
| 0x0f00 | System control |

Its interrupts are Chairman sources 0 to 7, leaving 8 to 30 for other
devices and 31 for the timer:

| Source | Raised when |
|---|---|
| 0 | UART 0 has received data, or has room to send, as its enables say |
| 1 | UART 1, likewise |
| 2 | An SPI transfer has completed |
| 3 | A GPIO line changed as its enables say |
| 4 | The real-time clock reached its alarm |
| 5 to 7 | Reserved |

Every source is cleared where it is raised, in the part's own status
register, as well as in the Chairman's pending register.

### 6.1 Identification and clock

| Offset | Access | Register |
|---|---|---|
| 0x0000 | R | Identification: bits 7:0 the IOC revision, bits 15:8 the number of UARTs (0 to 2), bits 23:16 the number of GPIO lines (0 to 32), bit 24 set if the SPI master is present, bit 25 set if the real-time clock is present, bits 31:26 reserved |
| 0x0004 | R | Clock frequency in Hz: the clock the UART baud rate divisors, the SPI divisor and the counter run from |

### 6.2 UARTs

UART `n` is at `0x0100 + 0x100 * n`.  Each has a receive FIFO and a
transmit FIFO of at least 16 bytes; the status register says how deep
they are, so software need not assume.

| Offset | Access | Register |
|---|---|---|
| +0x00 | R | Status: bit 0 receive data waiting, bit 1 room to transmit, bit 2 transmitter idle, bit 3 receive overrun since last cleared, bit 4 framing error since last cleared, bits 15:8 bytes waiting in the receive FIFO, bits 23:16 the FIFO depth, bits 31:24 reserved |
| +0x04 | RW | Data: a read takes the next received byte in bits 7:0 (undefined if none is waiting); a write queues bits 7:0 to send (dropped if there is no room) |
| +0x08 | RW | Baud rate divisor: the bit rate is the clock frequency divided by 16 and by the divisor plus one |
| +0x0c | RW | Interrupt enable: bit 0 raise the UART's source while data is waiting, bit 1 raise it while there is room to transmit |
| +0x10 | W | Clear: writing a word with bit 3 or bit 4 set clears that error bit in the status register |

The format is 8 data bits, no parity, one stop bit; the divisor is 0 at
reset and the interrupt enables are 0.  The UART's interrupt source is
raised while either enabled condition holds, so a handler that takes
all the data or fills the transmitter clears it by that alone, and
should disable the transmit interrupt when it has nothing more to send.

UART 0 is the console: a system with an IOC delivers the Chairman's
serial console (section 5.4) through it, and the Chairman's serial
registers are then not present.

### 6.3 SPI master

One byte at a time, which is enough for an SD card in SPI mode; the
divisor sets the clock, and the chip select is a bit software drives,
so a transaction of any length is a matter of holding it.

| Offset | Access | Register |
|---|---|---|
| +0x00 | RW | Control: bit 0 enable, bit 1 clock polarity (CPOL), bit 2 clock phase (CPHA), bit 3 assert chip select (active low on the pin), bits 15:8 clock divisor: the SPI clock is the clock frequency divided by 2 and by the divisor plus one, bit 16 raise the interrupt source on completion, others reserved |
| +0x04 | RW | Data: a write starts sending bits 7:0 while receiving a byte; a read gives the byte received by the last transfer |
| +0x08 | R | Status: bit 0 busy, bit 1 a transfer has completed since the status was last read, which reading clears |

Writing the data register while busy is ignored.  A second SPI master,
if a system wants one, is at 0x0380 with the same layout.

### 6.4 GPIO

Up to 32 lines; the identification register says how many, counting
from line 0.  A line is an input unless its direction bit is set.

| Offset | Access | Register |
|---|---|---|
| +0x00 | RW | Direction: 1 for output |
| +0x04 | RW | Output: the value driven on output lines; a read gives what was written |
| +0x08 | R | Input: the level on every line, outputs included |
| +0x0c | W | Set: 1 bits set output bits |
| +0x10 | W | Clear: 1 bits clear output bits |
| +0x14 | RW | Interrupt on rise: 1 bits raise the GPIO source when that line goes high |
| +0x18 | RW | Interrupt on fall: likewise when it goes low |
| +0x1c | RW | Interrupt pending: which lines have raised it; writing a word clears the bits set in it |

The GPIO source is raised while any pending bit is set.

### 6.5 Real-time clock and counter

| Offset | Access | Register |
|---|---|---|
| +0x00 | RW | Seconds: counts up once a second from whatever was last written.  Seconds since 1970 by convention |
| +0x04 | RW | Alarm: when the seconds register reaches this value the clock's interrupt source is raised |
| +0x08 | R | Counter: a free-running count at the clock frequency, wrapping at 2^32 |
| +0x0c | RW | Status: bit 0 the alarm has been reached, cleared by writing a word with bit 0 set; bit 1 the seconds register was not kept while power was off and holds 0 |

The seconds register is what `time()` reads and what the operating
system's `settime` writes; a system with battery-backed time keeps it
across power, and says so by leaving status bit 1 clear.  The counter
gives clocks and delays a resolution the timer's tick does not.

### 6.6 System control

| Offset | Access | Register |
|---|---|---|
| +0x00 | W | Halt: writing 1 stops the CPU clock until an interrupt that is enabled in the Chairman arrives; writing 2 stops it for good, which is what a program that has finished does |
| +0x04 | W | Reset: writing 0x4d454f57 resets the system as power-on does |
| +0x08 | RW | LEDs: a bit per indicator the board has, for whatever the software means by them |

`BNV #-2`, which halts the simulator, has no effect on hardware; the
operating system halts through this register when it finds an IOC and
loops otherwise.
