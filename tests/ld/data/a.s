; code referencing data and bss in another file, and a DCD of a code symbol
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        IMPORT  table, buffer
        ENTRY   start
ret     MOV     pc, lr
start   LDR     r0, =table
        LDR     r1, =buffer
        LDR     r0, [r0]
        STR     r0, [r1]
loop    B       loop
        LTORG
        AREA    |.data|, DATA, READWRITE
count   DCD     7
        DCW     start
        DCB     ret
        ALIGN
