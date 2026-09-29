; loads, stores, writeback modes
        GET     msim.inc.s

start   LDR     r4, =0x08000000
        LDR     r0, =0x12345678
        STR     r0, [r4]
        MOV     r1, #-1
        LDRB    r1, [r4]                ; zero-extended byte
        PUTX    r1
        MOV     r1, #-1
        LDRH    r1, [r4]                ; zero-extended halfword
        PUTX    r1
        LDR     r1, =0x0000ffff
        LDRHH   r1, [r4]                ; high half only
        PUTX    r1
        MOV     r5, r4
        LDRH    r2, [r5], #2            ; word from two halves, low first
        LDRHH   r2, [r5]
        PUTX    r2
        PUTX    r5
        LDR     r0, =0xaabbccdd
        STRB    r0, [r4]
        LDR     r1, [r4]
        PUTX    r1
        STRH    r0, [r4]
        LDR     r1, [r4]
        PUTX    r1
        STRHH   r0, [r4]
        LDR     r1, [r4]
        PUTX    r1
        LDR     sp, =0x08000100
        MOV     r0, #1
        MOV     r1, #2
        PUSH    {r0, r1}                ; r1 at 0xfc, r0 at 0xf8
        PUTX    sp
        LDR     r2, [sp]
        PUTX    r2
        POP     {r2, r3}
        PUTX    r2
        PUTX    r3
        PUTX    sp
        LDR     r0, [sp, #-4]!          ; decrease before
        PUTX    r0
        PUTX    sp
        LDR     r0, [sp], #-4           ; decrease after
        PUTX    r0
        PUTX    sp
        LDRB    r6, [sp], #1
        PUTX    sp
        HALT    3
