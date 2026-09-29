; msim helpers for tests: output through the simulator's BNV calls
        MACRO   PUTC $ch
        LDI     #$ch
        BNV     #-6
        MEND

        MACRO   NEWLINE
        PUTC    10
        MEND

        MACRO   PUTX $reg              ; hex, no leading zeros
        MOV     ir, $reg
        BNV     #-10
        NEWLINE
        MEND

        MACRO   PUTI $reg              ; signed decimal
        MOV     ir, $reg
        BNV     #-8
        NEWLINE
        MEND

        MACRO   HALT $code=0
        LDI     #$code
        BNV     #-2
        MEND

        MACRO   COND $c, $ch           ; print $ch if condition $c holds
        B$c     .yes
        B       .no
.yes    PUTC    $ch
.no
        MEND

        MACRO   FLAGS                  ; one letter per condition that holds
        COND    EQ, 'E'
        COND    NE, 'n'
        COND    CS, 'C'
        COND    CC, 'c'
        COND    MI, 'M'
        COND    PL, 'p'
        COND    VS, 'V'
        COND    VC, 'v'
        COND    HI, 'H'
        COND    LS, 'l'
        COND    GE, 'G'
        COND    LT, 'L'
        COND    GT, 'g'
        COND    LE, 'e'
        NEWLINE
        MEND
