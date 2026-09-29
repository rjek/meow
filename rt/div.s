; MABI runtime: division and remainder
        AREA    |.text|, CODE, READONLY
        EXPORT  __udivmod
        EXPORT  __udiv
        EXPORT  __umod
        EXPORT  __udiv10
        EXPORT  __divmod
        EXPORT  __div
        EXPORT  __mod
        EXPORT  __div10
        EXPORT  __divtest

        MACRO   NEGATE $r
        MVN     $r, $r
        ADD     $r, #1
        MEND

; Divisor in a1, dividend in a2, as on Arm: this is the order the
; compiler evaluates them in.  Returns a1 = quotient, a2 = remainder.
; Corrupts a3, a4, at.
__udivmod
__udiv
        EOR     r2, r2                  ; quotient
        EOR     r3, r3                  ; remainder
        MOV     at, #32
.loop   LSL     r3, #1
        TST     r1, #0x80000000
        BEQ     .nobit
        ORR     r3, #1
.nobit  LSL     r1, #1
        LSL     r2, #1
        CMP     r3, r0
        BCC     .less
        SUB     r3, r0
        ORR     r2, #1
.less   SUB     at, #1
        CMP     at, #0
        BNE     .loop
        MOV     r0, r2
        MOV     r1, r3
        RET

__umod  PUSH    {lr}
        BL      __udivmod
        MOV     r0, r1
        POP     {pc}

__udiv10
        MOV     r1, r0
        MOV     r0, #10
        B       __udivmod

; signed: the quotient is negative if the signs differ, the remainder
; takes the sign of the dividend
__divmod
__div   PUSH    {v1, lr}
        EOR     v1, v1
        CMP     r1, #0
        BGE     .pos1
        NEGATE  r1
        EOR     v1, #3
.pos1   CMP     r0, #0
        BGE     .pos2
        NEGATE  r0
        EOR     v1, #1
.pos2   BL      __udivmod
        TST     v1, #1
        BEQ     .quot
        NEGATE  r0
.quot   TST     v1, #2
        BEQ     .done
        NEGATE  r1
.done   POP     {v1, pc}

__mod   PUSH    {lr}
        BL      __divmod
        MOV     r0, r1
        POP     {pc}

__div10 MOV     r1, r0
        MOV     r0, #10
        B       __divmod

; the compiler calls this with the divisor before dividing
__divtest
        CMP     r0, #0
        BNE     .ok
        LDI     #99
        BNV     #-2                     ; halt: division by zero
.ok     RET
