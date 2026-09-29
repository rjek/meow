; Program start-up under Catflap: the kernel enters at start(argc, argv);
; main's result goes to the C library's exit, which flushes and ends the
; process.
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        IMPORT  main
        IMPORT  exit
        ENTRY   start
start   LDR     r2, =main
        ADD     lr, pc, #4
        MOV     pc, r2
        LDR     r2, =exit
        MOV     pc, r2
        LTORG
