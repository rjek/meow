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

; rd = number of leading zeros of rs, by halving; rs and ir are spoilt
        MACRO   CLZ $rd, $rs
        EOR     $rd, $rd
        MOV     ir, $rs
        LSR     ir, #16
        CMP     ir, #0
        BNE     .a\@
        ADD     $rd, #16
        LSL     $rs, #16
.a\@    MOV     ir, $rs
        LSR     ir, #24
        CMP     ir, #0
        BNE     .b\@
        ADD     $rd, #8
        LSL     $rs, #8
.b\@    MOV     ir, $rs
        LSR     ir, #28
        CMP     ir, #0
        BNE     .c\@
        ADD     $rd, #4
        LSL     $rs, #4
.c\@    MOV     ir, $rs
        LSR     ir, #30
        CMP     ir, #0
        BNE     .d\@
        ADD     $rd, #2
        LSL     $rs, #2
.d\@    MOV     ir, $rs
        LSR     ir, #31
        CMP     ir, #0
        BNE     .e\@
        ADD     $rd, #1
.e\@
        MEND

; Divisor in a1, dividend in a2, as on Arm: this is the order the
; compiler evaluates them in.  Returns a1 = quotient, a2 = remainder.
; Corrupts a3, a4, at.
;
; The divisor is shifted up to line up with the dividend, then subtracted
; out a bit at a time, so the work is proportional to the size of the
; quotient rather than a fixed 32 steps.
__udivmod
__udiv  CMP     r1, r0
        BHS     .divide
        EOR     r0, r0                  ; quotient 0, remainder the dividend
        RET
.divide MOV     r2, r0
        CLZ     r3, r2                  ; r3 = clz(divisor)
        MOV     r2, r1
        CLZ     at, r2
        SUB     r3, at                  ; r3 = shift that lines them up
        MOV     r2, r0
        LSL     r2, r3                  ; r2 = divisor << shift
        MOV     at, #1
        LSL     at, r3                  ; at = quotient bit
        EOR     r0, r0
.loop   CMP     r1, r2
        BLO     .next
        SUB     r1, r2
        ORR     r0, at
.next   LSR     r2, #1
        LSR     at, #1
        CMP     at, #0
        BNE     .loop
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
