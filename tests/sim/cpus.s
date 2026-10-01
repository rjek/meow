; two CPUs: starting the second, a lock, local memory, the doorbell and
; WFI.  Every step is ordered through a word in RAM, so the output is the
; same however the simulator interleaves them.
        GET     msim.inc.s

        B       main
        ALIGN   32
irq     BNV     #2                      ; the vector is shared: who am I?
        MOV     r1, ir
        PUTC    'D'
        PUTI    r1
        LDR     r0, =0xf8002400         ; my own pending word
        LDR     r1, =0x40000000         ; the doorbell is source 30
        STR     r1, [r0]
        LDR     r0, =0x08000000
        MOV     r1, #2
        STR     r1, [r0]                ; step 2: told
        IRQRTN

main    LDR     r0, =0xf8002c00         ; CPUs present
        LDR     r0, [r0]
        PUTX    r0
        LDR     r4, =0xf8002e00         ; lock 0
        LDR     r0, [r4]                ; take it: 0 means taken
        CMP     r0, #0
        BNE     fail
        PUTC    'A'
        NEWLINE
        LDR     r5, =0x08000000         ; the mailbox
        MOV     r0, #0
        STR     r0, [r5]
        LDR     r0, =0xf8002824         ; CPU 1's start address
        LDR     r1, =second
        STR     r1, [r0]
        LDR     r0, =0xf8002828         ; and its control: go
        MOV     r1, #1
        STR     r1, [r0]
.wait1  LDR     r0, [r5]
        CMP     r0, #1
        BNE     .wait1                  ; step 1: it has run
        LDR     r0, =0xe8400000         ; CPU 1's local memory, from here
        LDR     r0, [r0]
        PUTX    r0
        LDR     r0, =0xf0000000         ; my own, untouched
        LDR     r0, [r0]
        PUTX    r0
        MOV     r0, #0
        STR     r0, [r4]                ; release the lock
        LDR     r0, =0xf800282c
        STR     r0, [r0]                ; ring CPU 1's doorbell
.wait2  LDR     r0, [r5]
        CMP     r0, #2
        BNE     .wait2                  ; step 2: it was told
        LDR     r0, =0xf8002820         ; CPU 1's status
        LDR     r1, [r0]
        PUTX    r1                      ; present and running
        LDR     r1, =0xf8002828
        MOV     r2, #0
        STR     r2, [r1]                ; stop it
        LDR     r1, [r0]
        PUTX    r1                      ; present
        LDR     r0, [r4]                ; the lock is free again
        CMP     r0, #0
        BNE     fail
        PUTC    'C'
        NEWLINE
        HALT    0
fail    PUTC    'F'
        NEWLINE
        HALT    1

second  BNV     #2
        PUTI    ir                      ; 1
        LDR     r4, =0xf8002e00
        LDR     r0, [r4]                ; held by CPU 0: reads 1
        CMP     r0, #1
        BNE     fail
        PUTC    'B'
        NEWLINE
        LDR     r0, =0xf0000000         ; my local memory
        LDR     r1, =0x1234
        STR     r1, [r0]
        LDR     r0, =0xf8002004         ; my mask: the doorbell
        LDR     r1, =0x40000000
        STR     r1, [r0]
        LDR     r5, =0x08000000
        MOV     r0, #1
        STR     r0, [r5]                ; step 1
.sleep  BNV     #6                      ; WFI, until stopped
        B       .sleep
