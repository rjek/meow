; mode: elf
        AREA    |.text|, CODE, READONLY
        EXPORT  main
        IMPORT  puts
        ENTRY   main
main    PUSH    {lr}
        LDR     r0, =message
        LDR     r1, =puts
        MOV     lr, pc
        ADD     lr, #4
        MOV     pc, r1
        POP     {pc}
        LTORG
        AREA    |.data|, DATA, READWRITE
message DCB     "hello", 0
        ALIGN
table   DCD     main, message + 2, puts, 42
        DCW     main
        AREA    |.bss|, BSS, ALIGN=8
buffer  SPACE   64
