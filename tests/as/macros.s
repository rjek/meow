        GET     common.inc.s

        MACRO   DELAY $count, $reg=r9
        MOV     $reg, #$count
.again  SUB     $reg, #1
        BNE     .again
        MEND

        MACRO   PAIR $a, $b
        DCB     $a, $b
        MEND

start   SYS     SYS_PUTC
        DELAY   10
        DELAY   1000, r8
.again  B       .again          ; the file-level .again, not a macro one
        PAIR    1, 2
        PAIR    'x', "y"

        IF      SYS_PUTC == 6
        DCB     1
        ELSE
        DCB     2
        ENDIF
        [ SYS_PUTC > 10
        DCB     3
        |
        DCB     4
        ]
        IF      0
        DCB     5
        ELSE IF 1
        DCB     6
        ELSE
        DCB     7
        ENDIF

n       SET     0
        WHILE   n < 3
        DCB     n * 2
n       SET     n + 1
        WEND
        ASSERT  n == 3
