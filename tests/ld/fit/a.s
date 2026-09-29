        AREA    |.text|, CODE, READONLY
        EXPORT  start
start   MOV     pc, lr
        AREA    |.data|, DATA, READWRITE
        DCB     start
