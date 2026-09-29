; branch and ADR relaxation
        B       near            ; short forward
        BEQ     far             ; conditional long forward
        BL      far             ; long call
        B       far             ; long jump
        ADR     r0, near        ; 1 word
        ADR     r1, far         ; 2 words
        ADR     ir, far         ; 2 words
        ADR     r2, back        ; 1 word backwards
        LDR     r3, =far        ; pool at the LTORG below
        LDR     r4, =0x0000abcd ; too big for MOV, goes in the pool
        LDR     r5, =0x00000123 ; fits LDI, becomes a MOV
        LDR     r6, =-1         ; fits LDI
        LDR     r7, =0x80000000 ; single bit, EOR + ORR
near    NOP
        LTORG
back    SPACE   600
far     B       back            ; long backwards
        BNE     far             ; short backwards
        BLNE    near
        DCD     .
