; What the C library needs from the machine, done with msim's BNV calls.
        AREA    |.text|, CODE, READONLY
        EXPORT  _PDCLIB_meow_putc
        EXPORT  _PDCLIB_meow_getc
        EXPORT  _PDCLIB_meow_halt
        EXPORT  _PDCLIB_meow_time
        EXPORT  _PDCLIB_meow_cycles

; void _PDCLIB_meow_putc(int c): one character to the console
_PDCLIB_meow_putc
        MOV     ir, r0
        BNV     #-6
        RET

; int _PDCLIB_meow_getc(void): one character from the console, -1 at
; the end of input
_PDCLIB_meow_getc
        BNV     #-12
        MOV     r0, ir
        RET

; void _PDCLIB_meow_halt(int status): stop, never returns
_PDCLIB_meow_halt
        MOV     ir, r0
        BNV     #-2
        B       _PDCLIB_meow_halt

; long _PDCLIB_meow_time(void): the host's time in seconds since 1970
_PDCLIB_meow_time
        BNV     #-14
        MOV     r0, ir
        RET

; unsigned long _PDCLIB_meow_cycles(void): instructions executed so far
_PDCLIB_meow_cycles
        BNV     #-16
        MOV     r0, ir
        RET
