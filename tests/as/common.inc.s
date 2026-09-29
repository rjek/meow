; shared definitions pulled in with GET
SYS_PUTC EQU    6
        MACRO   SYS $n
        LDI     #$n
        ADD     lr, pc, #4
        EOR     pc, pc
        MEND
