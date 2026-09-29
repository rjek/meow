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
