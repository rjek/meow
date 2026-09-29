; Marks where the shared C library's data begins.  The library's objects
; are linked after this one and before libc_end.o, and the kernel links
; with -B so that bss runs backwards: the library's data and bss then form
; one range, from __libc_data_start to __libc_data_end.
        AREA    |.data|, DATA, READWRITE
        EXPORT  __libc_data_start
__libc_data_start
        DCD     0
        AREA    |.bss|, NOINIT
        EXPORT  __libc_data_end
__libc_data_end
        SPACE   4
