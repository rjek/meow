; Program start-up for code compiled with nmcc and run under msim.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        EXPORT  exit
        IMPORT  main
        ENTRY   start

start   LDR     sp, =0x08010000         ; top of msim's 64 KB of RAM
        MOV     r0, #0                  ; argc
        MOV     r1, #0                  ; argv
        LDR     r2, =main
        ADD     lr, pc, #4
        MOV     pc, r2
                                        ; main's result is exit's argument
exit    MOV     ir, r0
        BNV     #-2                     ; msim: halt with ir as the status
        B       exit
        LTORG
