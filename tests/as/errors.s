        ADD     r0, r1, #0
        ADD     r0, r1, #16
        CMP     ar0, #1
        TST     r0, #3
        LDI     #2048
        LDR     r0, [r1], #2
        LDR     r0, [r0], #4
        MOV     ar0, #1
        LSL     r0, #32
        ADD     ir, #4096
        BNV     #3
        B       elsewhere
dup     NOP
dup     NOP
        AREA    other, DATA
elsewhere DCD   1
        AREA    |.text|
        ADR     r0, elsewhere
        PUSH    {}
        FOO     r0
        DCB     "hi
        IF      1
