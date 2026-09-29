; Program start-up under Catflap, before there is a C library: the
; kernel enters at start(argc, argv), main's result is the exit status.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        IMPORT  main
        IMPORT  process_exit
        ENTRY   start
start   LDR     r2, =main
        ADD     lr, pc, #4
        MOV     pc, r2
        LDR     r2, =process_exit
        MOV     pc, r2
        LTORG
