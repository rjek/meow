; Program start-up for code compiled with nmcc and run under msim.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        IMPORT  main
        IMPORT  exit
        IMPORT  __data_load
        IMPORT  __data_start
        IMPORT  __data_end
        IMPORT  __bss_start
        IMPORT  __bss_end
        ENTRY   start

start   LDR     r0, =0xF8000104         ; the Chairman's size word for chip 1
        LDR     r0, [r0]                ; msim's RAM, however much was asked for
        LDR     sp, =0x08000000
        ADD     sp, r0
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
.go     MOV     r0, #1                  ; argc: an empty program name
        LDR     r1, =argv               ; and nothing else
        LDR     r2, =main
        ADD     lr, pc, #4
        MOV     pc, r2
        LDR     r2, =exit               ; main's result is exit's argument
        MOV     pc, r2
        LTORG

        AREA    |.rodata|, DATA, READONLY
argv    DCD     name
        DCD     0
name    DCB     "", 0
        ALIGN
