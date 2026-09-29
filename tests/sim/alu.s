; arithmetic, moves, shifts and bit operations
        GET     msim.inc.s

start   MOV     r0, #100
        ADD     r0, #200
        PUTI    r0                      ; 300
        SUB     r0, #45
        PUTI    r0                      ; 255
        ADD     r1, r0, #15
        PUTI    r1                      ; 270
        SUB     r2, r0, #1
        PUTI    r2                      ; 254
        ADD     r0, r1
        PUTI    r0                      ; 525
        SUB     r0, r2
        PUTI    r0                      ; 271
        SUB     r0, #400
        PUTI    r0                      ; -129
        ADD     r0, #-16
        PUTI    r0                      ; -145
        LDR     r3, =0x11223344
        MOVB    r4, r3
        PUTX    r4                      ; 22114433
        MOVW    r4, r3
        PUTX    r4                      ; 33441122
        MOVBW   r4, r3
        PUTX    r4                      ; 44332211
        LDR     r5, =0x80000000
        ASR     r5, #4
        PUTX    r5                      ; f8000000
        LDR     r5, =0x80000000
        LSR     r5, #4
        PUTX    r5                      ; 8000000
        MOV     r5, #1
        LSL     r5, #31
        PUTX    r5                      ; 80000000
        MOV     r5, #1
        ROR     r5, #1
        PUTX    r5                      ; 80000000
        ROL     r5, #2
        PUTX    r5                      ; 2
        MOV     r6, #33
        MOV     r5, #0x10
        ROR     r5, r6                  ; low five bits: rotate by 1
        PUTX    r5                      ; 8
        MOV     r6, #0
        LSL     r5, r6
        PUTX    r5                      ; 8
        LDR     r5, =-256
        ASR     r5, #4
        PUTI    r5                      ; -16
        LDR     r0, =0xf0f0
        AND     r0, #0x10
        PUTX    r0                      ; 10
        ORR     r0, #0x100
        PUTX    r0                      ; 110
        EOR     r0, #0x10
        PUTX    r0                      ; 100
        MOV     r1, #0xff
        BIC     r1, #0x8
        PUTX    r1                      ; f7
        MOV     r2, #0
        ORN     r2, #1
        PUTX    r2                      ; fffffffe
        EON     r2, #1
        PUTX    r2                      ; 0
        MVN     r3, #1
        PUTX    r3                      ; fffffffe
        NOT     r3
        PUTX    r3                      ; 1
        MVN     r3, r0
        PUTX    r3                      ; fffffeff
        MOV     r4, #0xff
        AND     r4, r1
        PUTX    r4                      ; f7
        ORR     r4, r0
        PUTX    r4                      ; 1f7
        EOR     r4, r1
        PUTX    r4                      ; 100
        BIC     r4, r0
        PUTX    r4                      ; 0
        MOV     r4, #0x0f
        ORN     r4, r1                  ; 0x0f | ~0xf7 = 0x0f | 0xffffff08
        PUTX    r4                      ; ffffff0f
        EON     r4, r1                  ; ffffff0f ^ ffffff08
        PUTX    r4                      ; 7
        LDR     r7, =0x12345678
        PUTX    r7
        LDR     r7, =-5000
        PUTI    r7
        MOV     r7, #0xabcd0000
        PUTX    r7
        MOV     ir, #0x12345678
        PUTX    ir
        HALT
