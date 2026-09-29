; Console output for C programs run under msim, through its BNV calls.
        AREA    |.text|, CODE, READONLY
        EXPORT  putchar
        EXPORT  puts
        EXPORT  print_int
        EXPORT  print_hex

putchar MOV     ir, r0
        BNV     #-6
        RET

puts    MOV     r1, r0
.loop   LDRB    r0, [r1], #1
        CMP     r0, #0
        BEQ     .done
        MOV     ir, r0
        BNV     #-6
        B       .loop
.done   MOV     ir, #10
        BNV     #-6
        MOV     r0, #0
        RET

print_int
        MOV     ir, r0
        BNV     #-8
        RET

print_hex
        MOV     ir, r0
        BNV     #-10
        RET
