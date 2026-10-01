; the IOC's UART 0 is the console: write through it, read standard input
; through it until the break that is its end, and look at UART 1's
; loopback.  Output goes through the UART rather than msim's BNVs.
        GET     msim.inc.s

UART0   EQU     0x10000100
UART1   EQU     0x10000200

main    LDR     r4, =UART0
        LDR     r5, =UART1
        LDR     r0, =0xf8000200         ; chip select 2's device
        LDR     r0, [r0]
        CMP     r0, #3
        BNE     fail
        LDR     r0, =0xf8002410         ; the Chairman's console is gone
        LDR     r0, [r0]
        CMP     r0, #0
        BNE     fail
        MOV     r0, #'>'
        BL      put
.loop   LDR     r0, [r4]                ; status
        TST     r0, #1                  ; data first, whatever else
        BNE     .byte
        TST     r0, #0x20               ; break: the end
        BNE     .end
        B       .loop
.byte   LDR     r0, =UART0+4
        LDR     r0, [r0]
        ADD     r0, #1                  ; each byte comes back one up
        BL      put
        B       .loop
.end    MOV     r0, #0x20
        LDR     r1, =UART0+0x10
        STR     r0, [r1]                ; clear the break
        LDR     r0, [r4]
        TST     r0, #0x20
        BNE     fail
        MOV     r0, #10
        BL      put
        LDR     r8, =UART1+4            ; loopback: three bytes
        MOV     r0, #'a'
        STR     r0, [r8]
        MOV     r0, #'b'
        STR     r0, [r8]
        MOV     r0, #'c'
        STR     r0, [r8]
        LDR     r0, [r5]
        LSR     r0, #8
        AND     r0, #0xff
        PUTI    r0                      ; 3 waiting
        LDR     r0, [r8]
        BL      put
        LDR     r0, [r8]
        BL      put
        LDR     r0, [r8]
        BL      put
        MOV     r0, #10
        BL      put
        LDR     r0, [r5]
        AND     r0, #1
        PUTI    r0                      ; 0: empty
        HALT    0
fail    PUTC    'F'
        HALT    1

put     LDR     r1, =UART0+4
        STR     r0, [r1]
        RET
