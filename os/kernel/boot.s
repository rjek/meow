; Catflap: reset and interrupt vectors, the context switch, and the few
; things C cannot say.  This object is linked first, so its first
; instruction is the reset vector at 0 and the interrupt vector is at 32.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        EXPORT  kernel_halt
        EXPORT  cpu_id
        IMPORT  kmain
        IMPORT  irq_dispatch
        IMPORT  switch_from
        IMPORT  switch_to
        IMPORT  __data_load
        IMPORT  __data_start
        IMPORT  __data_end
        IMPORT  __bss_start
        IMPORT  __bss_end
        ENTRY   start

RAM_BASE        EQU     0x08000000
CS_RAM_SIZE     EQU     0xF8000104      ; Chairman: size word of chip 1
IRQ_STACK       EQU     4096            ; below the boot stack at the top of RAM

start   B       reset
        SPACE   30                      ; the interrupt vector is at 32
        B       irq
        SPACE   30                      ; 64: reserved for the system call table
        SPACE   64

reset   LDR     r0, =CS_RAM_SIZE
        LDR     r0, [r0]
        LDR     r1, =RAM_BASE
        ADD     r1, r0                  ; top of RAM
        MOV     sp, r1                  ; the boot thread's stack, which becomes idle's
        LDR     r0, =IRQ_STACK
        SUB     r1, r0
        MOV     asp, r1                 ; the interrupt bank's, kept between interrupts
        LDR     r0, =__data_load
        LDR     r1, =__data_start
        LDR     r2, =__data_end
        CMP     r0, r1
        BEQ     .clear
.copy   CMP     r1, r2
        BHS     .clear
        LDR     r3, [r0], #4
        STR     r3, [r1], #4
        B       .copy
.clear  LDR     r1, =__bss_start
        LDR     r2, =__bss_end
        MOV     r3, #0
.zero   CMP     r1, r2
        BHS     .go
        STR     r3, [r1], #4
        B       .zero
.go     LDR     r0, =kmain
        MOV     pc, r0                  ; never returns

; The interrupt vector lands here in the interrupt bank, whose sp was set
; at reset.  irq_dispatch() decides whether to switch threads and says so
; through switch_from and switch_to; the registers of the interrupted
; thread are the other bank's, read and written as ar0 to apc.
irq     LDR     r0, =irq_dispatch
        ADD     lr, pc, #4
        MOV     pc, r0
        LDR     r1, =switch_to
        LDR     r1, [r1]
        CMP     r1, #0
        BEQ     .done
        LDR     r0, =switch_from
        LDR     r0, [r0]
        CMP     r0, #0
        BEQ     .load
        MOV     r2, ar0                 ; regs[] is at offset 0 of a thread
        STR     r2, [r0], #4
        MOV     r2, ar1
        STR     r2, [r0], #4
        MOV     r2, ar2
        STR     r2, [r0], #4
        MOV     r2, ar3
        STR     r2, [r0], #4
        MOV     r2, ar4
        STR     r2, [r0], #4
        MOV     r2, ar5
        STR     r2, [r0], #4
        MOV     r2, ar6
        STR     r2, [r0], #4
        MOV     r2, ar7
        STR     r2, [r0], #4
        MOV     r2, ar8
        STR     r2, [r0], #4
        MOV     r2, ar9
        STR     r2, [r0], #4
        MOV     r2, ar10
        STR     r2, [r0], #4
        MOV     r2, asp
        STR     r2, [r0], #4
        MOV     r2, alr
        STR     r2, [r0], #4
        MOV     r2, air
        STR     r2, [r0], #4
        MOV     r2, asr
        STR     r2, [r0], #4
        MOV     r2, apc
        STR     r2, [r0], #4
.load   LDR     r2, [r1], #4
        MOV     ar0, r2
        LDR     r2, [r1], #4
        MOV     ar1, r2
        LDR     r2, [r1], #4
        MOV     ar2, r2
        LDR     r2, [r1], #4
        MOV     ar3, r2
        LDR     r2, [r1], #4
        MOV     ar4, r2
        LDR     r2, [r1], #4
        MOV     ar5, r2
        LDR     r2, [r1], #4
        MOV     ar6, r2
        LDR     r2, [r1], #4
        MOV     ar7, r2
        LDR     r2, [r1], #4
        MOV     ar8, r2
        LDR     r2, [r1], #4
        MOV     ar9, r2
        LDR     r2, [r1], #4
        MOV     ar10, r2
        LDR     r2, [r1], #4
        MOV     asp, r2
        LDR     r2, [r1], #4
        MOV     alr, r2
        LDR     r2, [r1], #4
        MOV     air, r2
        LDR     r2, [r1], #4
        MOV     asr, r2
        LDR     r2, [r1], #4
        MOV     apc, r2
.done   BNV     #4                      ; IRQRTN

; void kernel_halt(int status): stop the machine.  Under msim the process
; exits with the status; real hardware would loop here.
kernel_halt
        MOV     ir, r0
        BNV     #-2
        B       kernel_halt

; int cpu_id(void): this CPU's bus ID
cpu_id  BNV     #2
        MOV     r0, ir
        AND     r0, #0x1f
        RET
        LTORG
