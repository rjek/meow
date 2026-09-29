; data linked to run in RAM while the image stores it after the code
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        IMPORT  __data_load, __data_start, __data_end, __bss_start, __bss_end
        ENTRY   start
start   LDR     r0, =__data_load
        LDR     r1, =__data_start
        LDR     r2, =__data_end
        LDR     r3, =__bss_start
        LDR     r4, =__bss_end
        LDR     r5, =table
loop    B       loop
        LTORG
        AREA    |.data|, DATA, READWRITE
table   DCD     start, table, buffer
        AREA    |.bss|, BSS
buffer  SPACE   8
