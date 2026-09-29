; exit() for programs without the C library: halt msim with the status.
; The C library has its own, which runs atexit handlers first, so this is
; not linked with it.
        AREA    |.text|, CODE, READONLY
        EXPORT  exit
exit    MOV     ir, r0
        BNV     #-2                     ; msim: halt with ir as the status
        B       exit
