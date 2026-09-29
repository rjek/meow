; timer interrupts through the Chairman
        GET     msim.inc.s

        B       main
        ALIGN   32
irq     PUTC    '!'
        STR     r9, [r10]               ; clear the pending bit
        SUB     r8, #1
        CMP     r8, #0
        BNE     .done
        NEWLINE
        HALT    5
.done   IRQRTN

main    LDR     r0, =0xf8002400         ; pending register
        MOV     ar10, r0
        LDR     r0, =0x80000000         ; timer is interrupt 31
        MOV     ar9, r0
        MOV     r0, #5
        MOV     ar8, r0
        LDR     r0, =0xf8002000         ; CPU 0 mask
        LDR     r1, =0x80000000
        STR     r1, [r0]
        LDR     r0, =0xf8002408         ; timer reload
        MOV     r1, #100
        STR     r1, [r0]
loop    B       loop
