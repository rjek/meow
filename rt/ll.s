; MABI runtime: 64-bit integers, as the compiler calls them.
; A long long travels in a register pair, low word first: a in a1:a2,
; b in a3:a4, the result in a1:a2.  The reverse forms (rsb, rdv, rrem)
; compute b OP a.
        AREA    |.text|, CODE, READONLY
        EXPORT  _ll_not
        EXPORT  _ll_neg
        EXPORT  _ll_add
        EXPORT  _ll_sub
        EXPORT  _ll_rsb
        EXPORT  _ll_and
        EXPORT  _ll_or
        EXPORT  _ll_eor
        EXPORT  _ll_shift_l
        EXPORT  _ll_ushift_r
        EXPORT  _ll_sshift_r
        EXPORT  _ll_from_l
        EXPORT  _ll_from_u
        EXPORT  _ll_to_l
        EXPORT  _ll_cmpeq
        EXPORT  _ll_cmpne
        EXPORT  _ll_ucmpgt
        EXPORT  _ll_ucmpge
        EXPORT  _ll_ucmplt
        EXPORT  _ll_ucmple
        EXPORT  _ll_scmpgt
        EXPORT  _ll_scmpge
        EXPORT  _ll_scmplt
        EXPORT  _ll_scmple
        EXPORT  _ll_mul
        EXPORT  _ll_udiv
        EXPORT  _ll_urem
        EXPORT  _ll_urdv
        EXPORT  _ll_urrem
        EXPORT  _ll_sdiv
        EXPORT  _ll_srem
        EXPORT  _ll_srdv
        EXPORT  _ll_srrem

; swap a and b
        MACRO   SWAP
        MOV     at, r0
        MOV     r0, r2
        MOV     r2, at
        MOV     at, r1
        MOV     r1, r3
        MOV     r3, at
        MEND

; negate the pair $lo:$hi in place
        MACRO   NEG64 $lo, $hi
        MVN     $lo, $lo
        MVN     $hi, $hi
        ADD     $lo, #1
        CMP     $lo, #0
        BNE     .n\@
        ADD     $hi, #1
.n\@
        MEND

; $lo:$hi += $blo:$bhi
        MACRO   ADD64 $lo, $hi, $blo, $bhi
        ADD     $lo, $blo
        CMP     $lo, $blo
        BHS     .n\@
        ADD     $hi, #1
.n\@   ADD     $hi, $bhi
        MEND

; $lo:$hi -= $blo:$bhi
        MACRO   SUB64 $lo, $hi, $blo, $bhi
        CMP     $lo, $blo
        SUB     $lo, $blo
        SUB     $hi, $bhi
        BHS     .n\@
        SUB     $hi, #1
.n\@
        MEND

; $lo:$hi <<= 1
        MACRO   SHL64 $lo, $hi
        LSL     $hi, #1
        TST     $lo, #0x80000000
        BEQ     .n\@
        ORR     $hi, #1
.n\@   LSL     $lo, #1
        MEND

; $lo:$hi >>= 1, zeros in
        MACRO   SHR64 $lo, $hi
        LSR     $lo, #1
        TST     $hi, #1
        BEQ     .n\@
        ORR     $lo, #0x80000000
.n\@   LSR     $hi, #1
        MEND

_ll_not MVN     r0, r0
        MVN     r1, r1
        RET

_ll_neg NEG64   r0, r1
        RET

_ll_add ADD64   r0, r1, r2, r3
        RET

_ll_sub SUB64   r0, r1, r2, r3
        RET

_ll_rsb SWAP
        B       _ll_sub

_ll_and AND     r0, r2
        AND     r1, r3
        RET

_ll_or  ORR     r0, r2
        ORR     r1, r3
        RET

_ll_eor EOR     r0, r2
        EOR     r1, r3
        RET

; shifts take the count in a3, low six bits
_ll_shift_l
        AND     r2, #0x3f
        CMP     r2, #0
        BEQ     .done
        CMP     r2, #32
        BLO     .small
        MOV     r1, r0
        SUB     r2, #32
        LSL     r1, r2
        MOV     r0, #0
        RET
.small  LSL     r1, r2
        MOV     at, #32
        SUB     at, r2
        MOV     ir, r0
        LSR     ir, at
        ORR     r1, ir
        LSL     r0, r2
.done   RET

_ll_ushift_r
        AND     r2, #0x3f
        CMP     r2, #0
        BEQ     .done
        CMP     r2, #32
        BLO     .small
        MOV     r0, r1
        SUB     r2, #32
        LSR     r0, r2
        MOV     r1, #0
        RET
.small  LSR     r0, r2
        MOV     at, #32
        SUB     at, r2
        MOV     ir, r1
        LSL     ir, at
        ORR     r0, ir
        LSR     r1, r2
.done   RET

_ll_sshift_r
        AND     r2, #0x3f
        CMP     r2, #0
        BEQ     .done
        CMP     r2, #32
        BLO     .small
        MOV     r0, r1
        SUB     r2, #32
        ASR     r0, r2
        ASR     r1, #31
        RET
.small  LSR     r0, r2
        MOV     at, #32
        SUB     at, r2
        MOV     ir, r1
        LSL     ir, at
        ORR     r0, ir
        ASR     r1, r2
.done   RET

_ll_from_l
        MOV     r1, r0
        ASR     r1, #31
        RET

_ll_from_u
        MOV     r1, #0
        RET

_ll_to_l
        RET

; comparisons return 0 or 1 in a1
_ll_cmpeq
        EOR     r0, r2
        EOR     r1, r3
        ORR     r0, r1
        CMP     r0, #0
        BEQ     .yes
        MOV     r0, #0
        RET
.yes    MOV     r0, #1
        RET

_ll_cmpne
        EOR     r0, r2
        EOR     r1, r3
        ORR     r0, r1
        CMP     r0, #0
        BNE     .yes
        MOV     r0, #0
        RET
.yes    MOV     r0, #1
        RET

; a > b unsigned: r0 = 1 if so
_ll_ucmpgt
        CMP     r1, r3
        BHI     .yes
        BLO     .no
        CMP     r0, r2
        BHI     .yes
.no     MOV     r0, #0
        RET
.yes    MOV     r0, #1
        RET

_ll_ucmple
        PUSH    {lr}
        BL      _ll_ucmpgt
        EOR     r0, #1
        POP     {pc}

_ll_ucmplt
        SWAP
        B       _ll_ucmpgt

_ll_ucmpge
        SWAP
        B       _ll_ucmple

; a > b signed: the high words compare signed, the low ones unsigned
_ll_scmpgt
        CMP     r1, r3
        BGT     .yes
        BLT     .no
        CMP     r0, r2
        BHI     .yes
.no     MOV     r0, #0
        RET
.yes    MOV     r0, #1
        RET

_ll_scmple
        PUSH    {lr}
        BL      _ll_scmpgt
        EOR     r0, #1
        POP     {pc}

_ll_scmplt
        SWAP
        B       _ll_scmpgt

_ll_scmpge
        SWAP
        B       _ll_scmple

; a * b, low 64 bits: shift and add over the bits of b
_ll_mul PUSH    {v1, v2, v3}
        MOV     v1, r0
        MOV     v2, r1
        MOV     r0, #0
        MOV     r1, #0
        MOV     v3, #64
.loop   TST     r2, #1
        BEQ     .skip
        ADD64   r0, r1, v1, v2
.skip   SHL64   v1, v2
        SHR64   r2, r3
        SUB     v3, #1
        CMP     v3, #0
        BNE     .loop
        POP     {v1, v2, v3}
        RET

; a / b unsigned: quotient in a1:a2, remainder in a3:a4.  Corrupts v1-v3.
; Long division, the quotient bits shifting in as the dividend leaves.
ll_udivmod
        MOV     v1, #0                  ; remainder
        MOV     v2, #0
        MOV     v3, #64
.loop   SHL64   v1, v2
        TST     r1, #0x80000000
        BEQ     .nobit
        ORR     v1, #1
.nobit  SHL64   r0, r1
        CMP     v2, r3                  ; remainder >= divisor?
        BHI     .sub
        BLO     .next
        CMP     v1, r2
        BLO     .next
.sub    SUB64   v1, v2, r2, r3
        ORR     r0, #1
.next   SUB     v3, #1
        CMP     v3, #0
        BNE     .loop
        MOV     r2, v1
        MOV     r3, v2
        RET

_ll_udiv
        PUSH    {v1, v2, v3, lr}
        BL      ll_udivmod
        POP     {v1, v2, v3, pc}

_ll_urem
        PUSH    {v1, v2, v3, lr}
        BL      ll_udivmod
        MOV     r0, r2
        MOV     r1, r3
        POP     {v1, v2, v3, pc}

_ll_urdv
        SWAP
        B       _ll_udiv

_ll_urrem
        SWAP
        B       _ll_urem

; signed: v4 bit 0 negates the quotient, bit 1 the remainder
ll_sdivmod
        MOV     v4, #0
        CMP     r1, #0
        BGE     .pos1
        NEG64   r0, r1
        EOR     v4, #3
.pos1   CMP     r3, #0
        BGE     .pos2
        NEG64   r2, r3
        EOR     v4, #1
.pos2   PUSH    {lr}
        BL      ll_udivmod
        POP     {lr}
        TST     v4, #1
        BEQ     .quot
        NEG64   r0, r1
.quot   TST     v4, #2
        BEQ     .done
        NEG64   r2, r3
.done   RET

_ll_sdiv
        PUSH    {v1, v2, v3, v4, lr}
        BL      ll_sdivmod
        POP     {v1, v2, v3, v4, pc}

_ll_srem
        PUSH    {v1, v2, v3, v4, lr}
        BL      ll_sdivmod
        MOV     r0, r2
        MOV     r1, r3
        POP     {v1, v2, v3, v4, pc}

_ll_srdv
        SWAP
        B       _ll_sdiv

_ll_srrem
        SWAP
        B       _ll_srem
