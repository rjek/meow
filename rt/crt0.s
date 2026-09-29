; Program start-up for code compiled with nmcc and run under msim.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        EXPORT  exit
        IMPORT  main
        IMPORT  __data_load
        IMPORT  __data_start
        IMPORT  __data_end
        IMPORT  __bss_start
        IMPORT  __bss_end
        ENTRY   start

start   LDR     sp, =0x08010000         ; top of msim's 64 KB of RAM
        LDR     r0, =__data_load
        LDR     r1, =__data_start
        LDR     r2, =__data_end
        CMP     r0, r1
        BEQ     .clear                  ; already in place
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
.go     MOV     r0, #0                  ; argc
        MOV     r1, #0                  ; argv
        LDR     r2, =main
        ADD     lr, pc, #4
        MOV     pc, r2
                                        ; main's result is exit's argument
exit    MOV     ir, r0
        BNV     #-2                     ; msim: halt with ir as the status
        B       exit
        LTORG
