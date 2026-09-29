; cross-file tail call through a literal pool
        AREA    |.text|, CODE, READONLY
        EXPORT  start
        IMPORT  func
start   MOV     r0, #1
        LDR     pc, =func
        LTORG
