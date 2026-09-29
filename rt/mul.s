; MABI runtime: a1 = a1 * a2
        AREA    |.text|, CODE, READONLY
        EXPORT  __mul

; Shift and add over the bits of the smaller operand, four bits a step,
; stopping when no bits are left.
__mul   CMP     r1, r0
        BLS     .go
        MOV     at, r0                  ; loop over the smaller of the two
        MOV     r0, r1
        MOV     r1, at
.go     MOV     r2, r0
        EOR     r0, r0
.loop   TST     r1, #1
        BEQ     .b1
        ADD     r0, r2
.b1     LSL     r2, #1
        TST     r1, #2
        BEQ     .b2
        ADD     r0, r2
.b2     LSL     r2, #1
        TST     r1, #4
        BEQ     .b3
        ADD     r0, r2
.b3     LSL     r2, #1
        TST     r1, #8
        BEQ     .b4
        ADD     r0, r2
.b4     LSL     r2, #1
        LSR     r1, #4
        CMP     r1, #0
        BNE     .loop
        RET

        EXPORT  __umull
; a1:a2 = a1 * a2, the full 64-bit product of two 32-bit values.  Shift
; and add over the bits of the smaller operand, four a step, into a
; 64-bit accumulator, stopping when no bits are left.
        MACRO   ADD64 $lo, $hi, $blo, $bhi
        ADD     $lo, $blo
        CMP     $lo, $blo
        BHS     .n\@
        ADD     $hi, #1
.n\@    ADD     $hi, $bhi
        MEND

        MACRO   SHL64 $lo, $hi
        LSL     $hi, #1
        TST     $lo, #0x80000000
        BEQ     .n\@
        ORR     $hi, #1
.n\@    LSL     $lo, #1
        MEND

__umull CMP     r1, r0
        BLS     .ugo
        MOV     at, r0
        MOV     r0, r1
        MOV     r1, at
.ugo    MOV     r2, r0                  ; r2:r3 = a, shifting up
        EOR     r3, r3
        MOV     at, r1                  ; at = b, shifting down
        EOR     r0, r0
        EOR     r1, r1
        CMP     at, #0
        BEQ     .udone
.uloop  TST     at, #1
        BEQ     .u1
        ADD64   r0, r1, r2, r3
.u1     SHL64   r2, r3
        TST     at, #2
        BEQ     .u2
        ADD64   r0, r1, r2, r3
.u2     SHL64   r2, r3
        TST     at, #4
        BEQ     .u3
        ADD64   r0, r1, r2, r3
.u3     SHL64   r2, r3
        TST     at, #8
        BEQ     .u4
        ADD64   r0, r1, r2, r3
.u4     SHL64   r2, r3
        LSR     at, #4
        CMP     at, #0
        BNE     .uloop
.udone  RET
