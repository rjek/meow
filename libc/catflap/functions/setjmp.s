; setjmp and longjmp: the callee-saved registers, sp and lr.
        AREA    |.text|, CODE, READONLY
        EXPORT  setjmp
        EXPORT  longjmp

setjmp  STR     r4, [r0], #4
        STR     r5, [r0], #4
        STR     r6, [r0], #4
        STR     r7, [r0], #4
        STR     r8, [r0], #4
        STR     r9, [r0], #4
        STR     sp, [r0], #4
        STR     lr, [r0]
        MOV     r0, #0
        MOV     pc, lr

longjmp LDR     r4, [r0], #4
        LDR     r5, [r0], #4
        LDR     r6, [r0], #4
        LDR     r7, [r0], #4
        LDR     r8, [r0], #4
        LDR     r9, [r0], #4
        LDR     sp, [r0], #4
        LDR     lr, [r0]
        MOV     r0, r1
        CMP     r0, #0
        BNE     .ret
        MOV     r0, #1                  ; longjmp with 0 returns 1
.ret    MOV     pc, lr
