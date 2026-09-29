        AREA    |.text|, CODE, READONLY
        EXPORT  helper
        IMPORT  start
helper  MOV     pc, lr
        AREA    |.data|, DATA, READWRITE
        EXPORT  table
table   DCD     start, helper, table, buffer
        AREA    |.bss|, BSS, ALIGN=8
        EXPORT  buffer
buffer  SPACE   16
