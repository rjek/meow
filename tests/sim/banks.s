; the alternative register bank
        GET     msim.inc.s

start   MOV     r0, #7
        MOV     ar1, r0
        MOV     r2, ar1
        PUTI    r2                      ; 7
        CMP     ar1, r0
        FLAGS                           ; equal
        MOV     r3, #9
        CMP     r0, ar1
        FLAGS
        CMP     ar1, ar1
        FLAGS
        TST     ar1, #4
        FLAGS                           ; not zero
        MOV     r3, asr
        AND     r3, #1
        PUTI    r3                      ; 1: the other bank is interrupt mode
        MOV     r3, sr
        AND     r3, #1
        PUTI    r3                      ; 0
        MOVW    ar2, r0
        MOV     r4, ar2
        PUTX    r4                      ; 70000
        HALT
