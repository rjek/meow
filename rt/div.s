; MABI runtime: division and remainder
        AREA    |.text|, CODE, READONLY
        EXPORT  __udivmod
        EXPORT  __udiv
        EXPORT  __umod
        EXPORT  __div
        EXPORT  __mod
        EXPORT  __divtest

        MACRO   NEGATE $r
        MVN     $r, $r
        ADD     $r, #1
        MEND

; a1 = a1 / a2, a2 = a1 % a2, unsigned.  Corrupts a3, a4, at.
__udivmod
        EOR     r2, r2                  ; quotient
        EOR     r3, r3                  ; remainder
        MOV     at, #32
.loop   LSL     r3, #1
        TST     r0, #0x80000000
        BEQ     .nobit
        ORR     r3, #1
.nobit  LSL     r0, #1
        LSL     r2, #1
        CMP     r3, r1
        BCC     .less
        SUB     r3, r1
        ORR     r2, #1
.less   SUB     at, #1
        CMP     at, #0
        BNE     .loop
        MOV     r0, r2
        MOV     r1, r3
        RET

__udiv  B       __udivmod

__umod  PUSH    {lr}
        BL      __udivmod
        MOV     r0, r1
        POP     {pc}

; signed: the quotient is negative if the signs differ, the remainder
; takes the sign of the dividend
__div   PUSH    {v1, lr}
        EOR     v1, v1
        CMP     r0, #0
        BGE     .pos1
        NEGATE  r0
        EOR     v1, #1
.pos1   CMP     r1, #0
        BGE     .pos2
        NEGATE  r1
        EOR     v1, #1
.pos2   BL      __udivmod
        TST     v1, #1
        BEQ     .done
        NEGATE  r0
.done   POP     {v1, pc}

__mod   PUSH    {v1, lr}
        EOR     v1, v1
        CMP     r0, #0
        BGE     .pos1
        NEGATE  r0
        EOR     v1, #1
.pos1   CMP     r1, #0
        BGE     .pos2
        NEGATE  r1
.pos2   BL      __udivmod
        MOV     r0, r1
        TST     v1, #1
        BEQ     .done
        NEGATE  r0
.done   RET

; the compiler calls this with the divisor before dividing
__divtest
        CMP     r0, #0
        BNE     .ok
        LDI     #99
        BNV     #-2                     ; halt: division by zero
.ok     RET
