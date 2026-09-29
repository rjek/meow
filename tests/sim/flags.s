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
        HALT
