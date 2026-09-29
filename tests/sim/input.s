; console input: count the characters and add up the digits until the end
        GET     msim.inc.s

start   MOV     r4, #0                  ; characters
        MOV     r5, #0                  ; sum of digits
.next   BNV     #-12
        CMP     ir, #-1
        BEQ     .done
        ADD     r4, #1
        MOV     r0, ir
        SUB     r0, #48
        CMP     r0, #10
        BHS     .next
        ADD     r5, r0
        B       .next
.done   PUTI    r4
        PUTI    r5
        BNV     #-12                    ; still at the end
        PUTX    ir
        HALT
