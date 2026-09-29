; The other end of the shared library's data: see libc_start.s
        AREA    |.data|, DATA, READWRITE
        EXPORT  __libc_data_mid
__libc_data_mid
        DCD     0
        AREA    |.bss|, NOINIT
        EXPORT  __libc_bss_first
__libc_bss_first
        SPACE   4
