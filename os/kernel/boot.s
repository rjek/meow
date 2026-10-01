; Catflap: reset and interrupt vectors, the context switch, and the few
; things C cannot say.  This object is linked first, so its first
; instruction is the reset vector at 0 and the interrupt vector is at 32.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        EXPORT  cpu_halt
        EXPORT  cpu_id
        EXPORT  kernel_time
        EXPORT  host_call
        EXPORT  cpu_model
        EXPORT  cpu_wfi
        EXPORT  cpu_entry
        EXPORT  __client_sb
        IMPORT  kmain
        IMPORT  cpu_main
        IMPORT  irq_dispatch
        IMPORT  __data_load
        IMPORT  __data_start
        IMPORT  __data_end
        IMPORT  __bss_start
        IMPORT  __bss_end
        ENTRY   start

RAM_BASE        EQU     0x08000000
CS_RAM_SIZE     EQU     0xF8000104      ; Chairman: size word of chip 1
IRQ_STACK       EQU     4096            ; below the boot stack at the top of RAM

; The CPU's own state is at the start of its local memory, laid out as
; kernel.h's struct cpu: the switch the handler is to make, the running
; process's displacement, which the shared library reads as __client_sb,
; and the stacks a CPU other than 0 starts with.
LOCAL           EQU     0xF0000000
SWITCH_FROM     EQU     0xF0000000
SWITCH_TO       EQU     0xF0000004
__client_sb     EQU     0xF0000008
BOOT_SP         EQU     0xF000000C
IRQ_SP          EQU     0xF0000010

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
        LDR     r1, =SWITCH_TO
        LDR     r1, [r1]
        CMP     r1, #0
        BEQ     .done
        LDR     r0, =SWITCH_FROM
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

; A CPU other than 0 starts here, with nothing in its registers, when CPU
; 0 has filled in its local memory and pressed its control register.
cpu_entry
        LDR     r0, =BOOT_SP
        LDR     r1, [r0]
        MOV     sp, r1
        LDR     r0, =IRQ_SP
        LDR     r1, [r0]
        MOV     asp, r1
        LDR     r0, =cpu_main
        MOV     pc, r0                  ; never returns

; void cpu_wfi(void): stop until an interrupt
cpu_wfi BNV     #6
        RET

; void cpu_halt(int status): stop the machine.  Under msim the process
; exits with the status; real hardware would loop here.
cpu_halt
        MOV     ir, r0
        BNV     #-2
        B       cpu_halt

;  long kernel_time(void): seconds since 1970, which under msim is the
; host's clock
kernel_time
        BNV     #-14
        MOV     r0, ir
        RET

; int host_call(int op, int a, int b, int c, int d): msim's hostfs, which
; takes the operation in r0 and its arguments in r1 to r4.  The fifth
; argument arrives on the stack.  With nothing there the BNV does nothing
; and ir keeps -ENOSYS.
host_call
        STR     r4, [sp, #-4]!
        LDR     r4, [sp, #4]
        LDI     #-38
        BNV     #-18
        MOV     r0, ir
        LDR     r4, [sp], #4
        RET

; int cpu_model(void): which implementation this is
cpu_model
        BNV     #0
        MOV     r0, ir
        LSR     r0, #8
        AND     r0, #0xff
        RET

; int cpu_id(void): this CPU's bus ID
cpu_id  BNV     #2
        MOV     r0, ir
        AND     r0, #0x1f
        RET
        LTORG
