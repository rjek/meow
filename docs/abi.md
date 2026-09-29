# MABI: the MEOW C application binary interface

MABI is the calling convention used by C code on MEOW.  It is modelled on
ARM's APCS, simplified where MEOW's instruction set makes the ARM choice
pointless.  It concerns only the active register bank: an interrupt
handler runs in the other bank and follows the same rules there.

## Registers

| Name | Number | Role | Saved by |
|---|---|---|---|
| a1 | r0 | Argument 1, integer result | Caller |
| a2 | r1 | Argument 2, second word of a 64-bit result | Caller |
| a3 | r2 | Argument 3 | Caller |
| a4 | r3 | Argument 4 | Caller |
| v1 to v6 | r4 to r9 | Register variables | Callee |
| at | r10 | Assembler and compiler temporary | Caller |
| sp | r11 | Stack pointer | Callee |
| lr | r12 | Link register | Caller |
| ir | r13 | Immediate register, scratch | Caller |
| sr | r14 | Status register; flags are not preserved across calls | |
| pc | r15 | Program counter | |

A function may destroy a1 to a4, at, lr and ir freely.  It must return
with v1 to v6 and sp holding the values they had on entry.  `ir` deserves
a warning: the assembler uses it as scratch when it synthesises large
immediates, `LDR =`, and long `ADR`s, so hand-written code cannot keep
anything in `ir` across such instructions.

There is no frame pointer.  A function that needs one may use any
callee-saved register and preserve it.

## Stack

`sp` points to a full-descending stack: it holds the address of the most
recently pushed word, and pushes decrement it first.  `sp` is a multiple
of 4 at every call and return.  There is no red zone: memory below `sp` may
be overwritten by an interrupt at any time.  Nothing checks for overflow.

`STR r, [sp, #-4]!` pushes and `LDR r, [sp], #4` pops; the assembler's
`PUSH {list}` and `POP {list}` expand to these with the lowest-numbered
register at the lowest address, so `PUSH {v1, v2, lr}` and `POP {v1, v2,
pc}` save and restore a frame and return.

## Data types

| Type | Size | Alignment |
|---|---|---|
| char | 1 | 1 |
| short | 2 | 2 |
| int, long, pointers, enum | 4 | 4 |
| long long | 8 | 4 |
| float | 4 | 4 |
| double, long double | 8 | 4 |

`char` is unsigned, because `LDRB` zero-extends and a signed byte load
costs two extra instructions.  Little-endian throughout.  Structure members
are aligned to their own alignment and structures to their most-aligned
member.  Bit-fields are allocated from the least significant bit of their
unit.  There is no floating-point hardware; `float` and `double` are IEEE
754 values manipulated by library calls.

## Calls and returns

A call sets `lr` to the return address and jumps.  The assembler's `BL`
does this within about 2 KB; beyond that, and for calls through pointers,
the sequence is:

```
        LDR     at, =function          ; or any register holding the address
        ADD     lr, pc, #4
        MOV     pc, at
```

A return is `MOV pc, lr` (the assembler's `RET`), or `POP {pc}` if `lr`
was pushed.

On entry to a function:

- `pc` is the function's first instruction;
- `lr` is the return address;
- `sp` is word aligned, and `[sp]` is the fifth argument word if there
  is one;
- a1 to a4 hold the first four argument words.

## Arguments

Arguments are converted to argument words: `char` and `short` are
promoted to `int`, `float` to `double` only for unprototyped and variadic
parameters, and every scalar occupies one word except `long long` and
`double`, which occupy two with the low word first.  Structures and unions
are passed by value as a sequence of words, padded to a whole number of
words.

The words are assigned in order to a1, a2, a3, a4 and then to the stack,
where the fifth word is at `[sp]`, the sixth at `[sp, 4]`, and so on.  A
two-word value that would straddle a4 and the stack is placed entirely on
the stack and a4 is left unused.  The callee may modify its stack
arguments; the caller pops them after the call.

Variadic functions receive their arguments in the same way.  The callee
stores a1 to a4 below its stack arguments on entry so that the whole list
is contiguous in memory for `va_arg`.

## Results

| Result | Where |
|---|---|
| Integer, pointer, `float` | a1 |
| `long long`, `double` | a1 (low word) and a2 (high word) |
| Structure or union | Memory: the caller passes the address of the result as a hidden first argument in a1, before the declared arguments, and the callee writes the result there and returns a1 unchanged |

## Runtime support

The instruction set has no multiply, divide, or 64-bit or floating-point
arithmetic.  Compiled code calls these functions, which follow MABI and
live in the runtime library:

| Function | Operation |
|---|---|
| `__mul` | a1 = a1 * a2 |
| `__div`, `__udiv` | a1 = a1 / a2, signed and unsigned |
| `__mod`, `__umod` | a1 = a1 % a2, signed and unsigned |
| `__divmod`, `__udivmod` | a1 = a1 / a2, a2 = a1 % a2 |
| `__mul64`, `__div64`, `__udiv64`, `__mod64`, `__umod64` | 64-bit versions taking two-word operands in a1:a2 and a3:a4 |
| `__lsl64`, `__lsr64`, `__asr64` | 64-bit shifts of a1:a2 by a3 |
| `__memcpy`, `__memset` | Block copy and fill with the C semantics |

Division by zero is undefined; the library may trap or return anything.
The floating-point entry points will be specified with the software
floating-point library.

## System calls

MABI says nothing about system calls; they belong to whatever runs on the
machine.  The convention used by earlier MEOW software, which an operating
system may adopt, is to place the call number in `ir` and jump to address
0 with the return address in `lr`, which is also where the CPU starts at
reset:

```
        LDI     #number
        ADD     lr, pc, #4
        EOR     pc, pc                  ; pc = 0
```
