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
unit, and a plain `int` bit-field is unsigned, as on Arm; write `signed
int` to get sign extension.  There is no floating-point hardware; `float`
and `double` are IEEE 754 values manipulated by library calls.  A `double`
is stored little-endian like everything else, low word at the lower
address, and travels in a register pair the same way up.

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

Arguments are converted to argument words: `char`, `short` and `float`
are passed as declared when the callee has a prototype (the caller
narrows) and promoted to `int` or `double` only for unprototyped and
variadic parameters, and every scalar occupies one word except `long
long` and `double`, which occupy two with the low word first.  Structures and unions
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
| `__div`, `__udiv` | a1 = a2 / a1 and a2 = a2 % a1, signed and unsigned |
| `__divmod`, `__udivmod` | Aliases of `__div` and `__udiv` |
| `__mod`, `__umod` | a1 = a2 % a1, signed and unsigned |
| `__div10`, `__udiv10` | a1 = a1 / 10 and a2 = a1 % 10, signed and unsigned |
| `__divtest` | Traps if a1 is zero; called before a division by a variable |

The division routines take the divisor first, as Arm's `__rt_sdiv` does,
because that is the order compilers find convenient to evaluate the
operands in.  Both quotient and remainder come back, so `x / y` and
`x % y` share one call.
| `_ll_add`, `_ll_sub`, `_ll_rsb`, `_ll_mul`, `_ll_and`, `_ll_or`, `_ll_eor` | 64-bit a OP b, a in a1:a2 and b in a3:a4, low word first; `rsb` is b - a |
| `_ll_udiv`, `_ll_urem`, `_ll_sdiv`, `_ll_srem` | 64-bit a / b and a % b; `_ll_urdv`, `_ll_urrem`, `_ll_srdv`, `_ll_srrem` compute b / a and b % a |
| `_ll_not`, `_ll_neg` | 64-bit complement and negation of a1:a2 |
| `_ll_shift_l`, `_ll_ushift_r`, `_ll_sshift_r` | 64-bit shifts of a1:a2 by a3 |
| `_ll_cmpeq`, `_ll_cmpne`, `_ll_ucmpgt`, `_ll_ucmpge`, `_ll_ucmplt`, `_ll_ucmple`, `_ll_scmpgt`, `_ll_scmpge`, `_ll_scmplt`, `_ll_scmple` | 64-bit comparisons of a with b, 0 or 1 in a1 |
| `_ll_from_l`, `_ll_from_u`, `_ll_to_l` | Widen a1 to a1:a2 with or without sign, and narrow back |
| `__memcpy`, `__memset` | Block copy and fill with the C semantics |

Division by zero is undefined; the library may trap or return anything.

Floating point is done by `rt/softfp.c`, plain C compiled with `nmcc`.
A `float` argument or result is its 32-bit pattern in one register and a
`double` is a register pair.  The names are the compiler's usual ones:

| Function | Operation |
|---|---|
| `_fadd`, `_fsub`, `_fmul`, `_fdiv`, `_fneg` | Single precision a + b, a - b, a * b, a / b, -a |
| `_frsb`, `_frdiv` | b - a and b / a |
| `_fgr`, `_fgeq`, `_fls`, `_fleq`, `_feq`, `_fneq` | Comparisons, 0 or 1 in a1; a NaN compares unequal to everything |
| `_fflt`, `_ffltu`, `_ffix`, `_ffixu` | `int` or `unsigned` to `float` and back, truncating |
| `_dadd` ... `_dneq`, `_dflt`, `_dfltu`, `_dfix`, `_dfixu` | The same for `double` |
| `_f2d`, `_d2f` | Widen and narrow |
| `_ll_sto_f`, `_ll_uto_f`, `_ll_sto_d`, `_ll_uto_d` | `long long` to floating |
| `_ll_sfrom_f`, `_ll_ufrom_f`, `_ll_sfrom_d`, `_ll_ufrom_d` | Floating to `long long`, truncating |

Rounding is to nearest, ties to even; denormals are kept.  A conversion to
an integer that does not fit gives the nearest representable extreme, and a
NaN gives zero; C leaves both undefined.

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
