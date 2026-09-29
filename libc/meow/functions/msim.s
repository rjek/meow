; What the C library needs from the machine, done with msim's BNV calls.
        AREA    |.text|, CODE, READONLY
        EXPORT  _PDCLIB_meow_putc
        EXPORT  _PDCLIB_meow_halt

; void _PDCLIB_meow_putc(int c): one character to the console
_PDCLIB_meow_putc
        MOV     ir, r0
        BNV     #-6
        RET

; void _PDCLIB_meow_halt(int status): stop, never returns
_PDCLIB_meow_halt
        MOV     ir, r0
        BNV     #-2
        B       _PDCLIB_meow_halt
