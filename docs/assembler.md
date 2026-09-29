# mas: the MEOW assembler

`mas` assembles MEOW source into a flat binary image or an ELF32
relocatable object.  Its syntax is modelled on ARM's ObjAsm, without the
RISC OS baggage, and with a few conveniences that ObjAsm lacks.

```
mas [-o output] [-f bin|elf] [-b base] [-l listing] [-M map]
    [-I dir] [-D name[=value]] [-Werror] source...
```

| Option | Meaning |
|---|---|
| `-o output` | Output file.  Default `out.bin`.  A name ending `.o` or `.elf` selects ELF unless `-f` says otherwise |
| `-f bin` or `-f elf` | Output format.  `bin` is a flat image; `elf` is a relocatable object for `mld` |
| `-b base` | Flat output only: address of the first byte of the image.  Default 0 |
| `-l listing` | Write a listing (`-` for standard output) |
| `-M map` | Write a map of sections and symbols (`-` for standard output) |
| `-I dir` | Add a directory to the search path for `GET` and `INCBIN` |
| `-D name[=value]` | Define an absolute symbol (`value` defaults to 1) |
| `-Werror` | Treat warnings as errors |

Several source files are assembled as if concatenated.  Diagnostics are
`file:line:col: error: message`; all errors in a file are reported, not
just the first.  The exit status is 1 if there were any errors and nothing
is written.

Two companion tools share its library: `mdis file.bin [base]` disassembles
a flat image, and `mobjdump [-d] file.o` prints the sections, symbols and
relocations of an ELF file, with `-d` disassembling its code.

## Source lines

A line is `[label] [operation [operands]] [; comment]`.  Anything after a
semicolon is a comment.

A **label** is an identifier in column 1, or an identifier followed by a
colon anywhere on the line.  Labels and symbols are case-sensitive;
mnemonics and directives are not.  Identifiers contain letters, digits,
`_`, `.` and `$`, and may not start with a digit.

A label beginning with a dot is **local**: its full name is the last
non-local label followed by the local name, so `.loop` after `main` is
`main.loop`.  Each macro expansion has its own local scope, so a macro
body can define `.again` freely.

## Registers

`r0` to `r15`, with the aliases `sp` (r11), `lr` (r12), `ir` (r13), `sr`
(r14) and `pc` (r15).  The ABI names `a1` to `a4` (r0 to r3), `v1` to `v6`
(r4 to r9) and `at` (r10) are also known.  A leading `a` selects the
alternative bank: `ar3`, `asp`, `apc`.  `name RN reg` defines another
name.

## Expressions

Operands are expressions using C syntax and precedence:

| Precedence | Operators |
|---|---|
| unary | `-` `+` `~` `!` |
| | `*` `/` `%` |
| | `+` `-` |
| | `<<` `>>` |
| | `<` `>` `<=` `>=` |
| | `==` `!=` |
| | `&` |
| | `^` |
| | `\|` |
| | `&&` |
| lowest | `\|\|` |

Numbers are decimal, `0x1f` or `&1f` hex, `0b101` binary, or a character
constant `'a'` (with C escapes).  Underscores in numbers are ignored.  `.`
is the address of the current instruction or data item.  Parentheses group.

A value is absolute, or an address in a section, or an imported symbol
plus an offset.  Two addresses in the same section may be subtracted to
give an absolute distance; other arithmetic on addresses is an error.

Immediates in instructions are written with a leading `#`.  Register
operands never are.

## Directives

| Directive | Meaning |
|---|---|
| `AREA name[, attr...]` | Select a section, creating it if needed.  Attributes: `CODE`, `DATA`, `BSS` (or `NOINIT`), `READONLY`, `READWRITE`, `ALIGN=n`.  The name may be written `\|name\|`.  If no `AREA` is given, code goes in `.text` |
| `name EQU expr` | Define a constant symbol.  Also `name * expr`.  The expression may use labels defined later |
| `name SET expr` | Define or redefine a variable.  Its value is captured wherever it is used, so it can count `WHILE` loops |
| `name RN register` | Name a register |
| `DCB values` | Bytes.  Values are expressions or `"strings"` (C escapes, no terminator added) |
| `DCW values` | 16-bit little-endian words |
| `DCD values` | 32-bit little-endian words.  Addresses of symbols in other sections or files become relocations |
| `SPACE n[, fill]` | Reserve `n` bytes.  Also `% n` |
| `ALIGN [n[, fill]]` | Pad to a multiple of `n` bytes (default 4) |
| `INCBIN file` | Insert the contents of a file |
| `GET file` | Assemble another file here.  Also `INCLUDE`.  Searched relative to the including file, then the `-I` directories |
| `EXPORT names` | Make symbols visible to the linker.  Also `GLOBAL` |
| `IMPORT names` | Declare symbols defined elsewhere.  Also `EXTERN` |
| `ENTRY [symbol]` | Mark the entry point (the current position if no symbol is given).  In ELF output it becomes the global symbol `__entry`, which `mld` uses |
| `LTORG` | Place the literal pool here (see `LDR =`) |
| `MACRO name [$p, $q=default...]` | Begin a macro definition, ended by `MEND` |
| `IF expr` `ELSE` `ENDIF` | Conditional assembly.  `[`, `\|` and `]` are accepted as well.  `ELSE IF expr` chains |
| `WHILE expr` `WEND` | Repeat the enclosed lines while the expression is non-zero |
| `ASSERT expr` | Error if the expression is zero |
| `INFO "text"`, `ERROR "text"` | Print a message; `ERROR` also fails the assembly |
| `END` | Stop reading the current file |

`IF` and `WHILE` conditions are evaluated as they are read, so they may
only use symbols already defined.

## Macros

```
        MACRO   DELAY $count, $reg=r9
        MOV     $reg, #$count
.again  SUB     $reg, #1
        BNE     .again
        MEND

        DELAY   1000
        DELAY   10, r8
```

Parameters are `$name`, replaced textually in the body; `$name.` allows a
parameter to be glued to following text.  Arguments are separated by
commas; brackets and quotes protect commas inside an argument.  A
parameter may have a default.  `\@` expands to a number unique to the
invocation, though local labels make it rarely necessary.  A label on the
invoking line is defined before the expansion.

## Instructions

The condition suffixes are those listed in the reference manual: `EQ`,
`NE`, `CS`/`HS`, `CC`/`LO`, `MI`, `PL`, `VS`, `VC`, `HI`, `LS`, `GE`,
`LT`, `GT`, `LE`.  Only `B` and `BL` take them; `BLT` is a branch on LT
and `BLLT` is a call on LT.

### Branches

| Syntax | Notes |
|---|---|
| `B{cond} label` | Range -512 to +510 bytes.  Out of range, `B` becomes `LDI`/`ADD pc, ir` (two words, range 2 KB) and `B{cond}` an inverted branch around that (three words).  Further than that is an error; use `LDR pc, =label` |
| `BL{cond} label` | `ADD lr, pc, #n` then the branch, expanded the same way.  `lr` is set even when a conditional call is not taken |
| `RET` | `MOV pc, lr` |
| `BNV #n` | Extension call, `n` even, -512 to 510 |
| `IRQRTN` | `BNV #4` |
| `NOP` | `MOV r0, r0` |

### Arithmetic

| Syntax | Notes |
|---|---|
| `ADD Rd, Rs` | Rd += Rs |
| `ADD Rd, Rs, #1..15` | Rd = Rs + imm.  `#0` is rejected; use `MOV` |
| `ADD Rd, #imm` | 0 to 255 directly; -255 to -1 becomes `SUB`; anything else is loaded into `ir` first (Rd may not be `ir`) |
| `SUB` | The same forms |
| `CMP Rn, #imm` | -128 to 127 directly, else via `ir` |
| `CMP {A}Rn, {A}Rm` | |
| `TST {A}Rn, #mask` | The mask must have exactly one bit set |

### Moves and constants

| Syntax | Notes |
|---|---|
| `MOV {A}Rd, {A}Rs` | Also `MOVB`, `MOVW`, `MOVBW` for the swapping forms |
| `MOV Rd, #imm` | Any 32-bit value.  Zero is `EOR Rd, Rd`; values that fit in 12 bits are `LDI` and `MOV`; a single set bit is `EOR` then `ORR`; others are built with `LDI`, `LSL` and `ADD`, up to eight words.  Uses `ir` |
| `LDI #imm` | -2048 to 2047 |
| `LDR Rd, =expr` | Load a 32-bit constant or address.  If it is cheaper as a `MOV` of at most two words, that is what is emitted; otherwise the value goes in the literal pool and is loaded with `ADR ir, literal` followed by `LDR Rd, [ir]` |
| `ADR Rd, label` | Address of a label in the same section into `Rd`: one word within 15 bytes, two within 255, three within 2 KB (two when `Rd` is `ir`).  Further is an error |

The literal pool is emitted at `LTORG` or at the end of the section.  It
must be within 2 KB of every `LDR =` that uses it, so long code needs an
`LTORG` after an unconditional branch now and then.

### Shifts and logic

| Syntax | Notes |
|---|---|
| `LSL`, `LSR`, `ASR`, `ROL`, `ROR Rd, #0..31` | `ASL` is `LSL` |
| `LSL`, `LSR`, `ASR`, `ROL`, `ROR Rd, Rs` | Low five bits of `Rs` |
| `AND`, `ORR`, `EOR`, `BIC`, `ORN`, `EON Rd, Rs` | |
| `AND`, `ORR`, `EOR`, `BIC`, `ORN`, `EON Rd, #mask` | A single-bit mask encodes directly; anything else goes through `ir` |
| `MVN Rd, Rs`, `MVN Rd, #mask` | Rd = NOT operand |
| `NOT Rd` | `MVN Rd, Rd` |

### Memory

| Syntax | Notes |
|---|---|
| `LDR Rv, [Ra]` | Word |
| `LDRB Rv, [Ra]` | Byte, zero-extended |
| `LDRH Rv, [Ra]` | Halfword into bits 15:0, zero-extended |
| `LDRHH Rv, [Ra]` | Halfword into bits 31:16, low half kept |
| `STR`, `STRB`, `STRH`, `STRHH` | The corresponding stores |
| `[Ra], #n` | Post-increment by the access size |
| `[Ra], #-n` | Post-decrement by the access size |
| `[Ra, #-n]!` | Pre-decrement by the access size |
| `PUSH {list}` | `STR r, [sp, #-4]!` for each register, highest first, so the lowest register ends at the lowest address |
| `POP {list}` | `LDR r, [sp], #4` for each register, lowest first.  `POP {pc}` returns |

`n` must equal the access size.  Register lists are written `{r4-r6, lr}`.

## Output and linking

Flat output lays the sections out consecutively from the base address in
the order they were first named, each aligned as declared (4 by default),
and resolves all references.  BSS sections at the end occupy no bytes in
the file.

ELF output keeps the sections separate and emits `ABS32`, `ABS16` and
`ABS8` relocations for `DCD`, `DCW` and `DCB` values and literal-pool
entries that refer to sections or imported symbols.  Branches and `ADR`
must stay within one section; reach code in another section or file with
`LDR pc, =symbol` (or `LDR Rd, =symbol` followed by `MOV pc, Rd`).  Link
with `mld`, described in `linker.md`.

## Differences from ObjAsm

For readers who know ObjAsm: expressions use C operators instead of
`:OR:`; local labels are `.name` rather than numbered; macros are declared
on one line with `MACRO name $a, $b`; `SET` replaces `SETA` and `GBLA`;
bit-operation immediates are masks with one bit set; there is no `LDR
Rd, [Ra, #offset]` because the hardware has no offset field; and the
memory-operand syntax describes only the four writeback modes the hardware
has.
