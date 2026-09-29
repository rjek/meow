; smoke test
        AREA    |.text|, CODE, READONLY
        EXPORT  start
count   EQU     3
tmp     RN      r4

start   MOV     r0, #0
        MOV     r1, #count
        LDI     #-1
        ADD     r2, r1, #5
        ADD     r2, r1
        SUB     r2, #7
        ADD     r3, #300
        CMP     r2, #-3
        CMP     ar1, r3
        TST     tmp, #0x100
        MOVB    r5, r6
        MOVBW   ar7, r8
        LSL     r1, #4
        ASR     r1, r2
        ROR     r1, #1
        AND     r1, r2
        ORR     r1, #1<<5
        BIC     r1, #0x80000000
        MVN     r1, r2
        NOT     r3
        EOR     r3, #0x300
        LDR     r0, [r1]
        LDRB    r0, [r1], #1
        STRH    r2, [r3], #-2
        STRHH   r2, [r3]
        STR     lr, [sp, #-4]!
        PUSH    {r4-r6, lr}
        POP     {r4-r6, pc}
        ADR     r0, table
        ADR     r1, start
        LDR     r2, =0x12345678
        LDR     r3, =table
        MOV     r5, #0x12345678
        MOV     ir, #-5000
.loop   SUB     r0, #1
        BNE     .loop
        BLEQ    far
        BL      far
        B       far
        BNV     #-6
        IRQRTN
        RET
        NOP
        LTORG
table   DCD     start, table, 0xdeadbeef, count * 2
        DCW     1, 2, -3
        DCB     "hi\n", 0
        ALIGN
        SPACE   6
        ALIGN   2
far     B       start
        ADDS    r1, #5
        SUBS    r2, #31
        ADDS    r3, r4
        SUBS    r5, r6
        END
