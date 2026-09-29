; MABI runtime: a1 = a1 * a2
        AREA    |.text|, CODE, READONLY
        EXPORT  __mul

__mul   MOV     r2, r0
        EOR     r0, r0
.loop   TST     r1, #1
        BEQ     .skip
        ADD     r0, r2
.skip   LSL     r2, #1
        LSR     r1, #1
        CMP     r1, #0
        BNE     .loop
        RET
