; branches, calls and returns across relaxation distances
        GET     msim.inc.s

start   PUTC    'a'
        BL      sub1
        PUTC    'c'
        B       far
back    PUTC    'e'
        MOV     r0, #0
        CMP     r0, #0
        BEQ     far2                    ; conditional long branch, taken
        PUTC    'X'
back2   CMP     r0, #1
        BEQ     far2                    ; not taken
        PUTC    'g'
        BLNE    sub1                    ; taken, long
        PUTC    'i'
        HALT
sub1    PUTC    'b'
        RET
        SPACE   700
far     PUTC    'd'
        B       back
far2    PUTC    'f'
        B       back2
