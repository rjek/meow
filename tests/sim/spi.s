; the IOC's SPI master with an SD card on it: CMD0 gets R1 = 1, and a
; transfer takes its time
        GET     msim.inc.s

SPI     EQU     0x10000300

main    LDR     r4, =SPI
        LDR     r5, =SPI+4
        LDR     r6, =SPI+8
        LDR     r0, =0x109              ; enabled, select, divisor 1
        STR     r0, [r4]
        MOV     r0, #0xff
        BL      xfer
        MOV     r0, #0x40               ; CMD0
        BL      xfer
        MOV     r0, #0
        BL      xfer
        BL      xfer
        BL      xfer
        BL      xfer
        MOV     r0, #0x95
        BL      xfer
        MOV     r0, #0xff
        BL      xfer                    ; Ncr
        PUTX    r0
        MOV     r0, #0xff
        BL      xfer
        PUTX    r0                      ; 1: idle
        LDR     r0, =0x101
        STR     r0, [r4]                ; deselect
        MOV     r0, #0x40
        BL      xfer                    ; nobody listening
        PUTX    r0                      ; ff
        MOV     r0, #0xff
        STR     r0, [r5]                ; busy for 32 cycles after this
        LDR     r0, [r6]
        AND     r0, #1
        PUTI    r0                      ; 1: busy
        MOV     r0, #0xff
        STR     r0, [r5]                ; ignored while busy
        MOV     r7, #0
.wait   ADD     r7, #1
        LDR     r0, [r6]
        TST     r0, #1
        BNE     .wait
        PUTI    r7                      ; how many looks
        HALT    0

xfer    STR     r0, [r5]
.busy   LDR     r0, [r6]
        TST     r0, #1
        BNE     .busy
        LDR     r0, [r5]
        RET
