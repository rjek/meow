; CMP and TST flag semantics (ARM-style C and V)
        GET     msim.inc.s

        MACRO   COMPARE $a, $b
        LDR     r0, =$a
        LDR     r1, =$b
        CMP     r0, r1
        FLAGS
        MEND

start   COMPARE 5, 3
        COMPARE 3, 5
        COMPARE 5, 5
        COMPARE -1, 1
        COMPARE 0x80000000, 1
        COMPARE 1, -1
        COMPARE 0x7fffffff, -1
        MOV     r0, #-3
        CMP     r0, #-3
        FLAGS
        LDR     r0, =0x80000000
        TST     r0, #0x80000000         ; N set, C and V untouched
        FLAGS
        TST     r0, #1                  ; Z set
        FLAGS
        LDR     r0, =0xffffffff         ; ADDS: carry out, no overflow
        ADDS    r0, #1
        FLAGS
        LDR     r0, =0x7fffffff         ; ADDS: signed overflow
        LDR     r1, =1
        ADDS    r0, r1
        FLAGS
        LDR     r0, =5                  ; SUBS is CMP that keeps the result
        SUBS    r0, #5
        FLAGS
        LDR     r0, =3
        LDR     r1, =5
        SUBS    r0, r1
        FLAGS
        CMP     r0, #-2                 ; the results were kept
        FLAGS
        HALT
