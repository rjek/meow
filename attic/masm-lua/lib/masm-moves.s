	; MEOW Standard assembler library

	; Moves of various forms

	MACRO	movw	$0	$1
	mov	wordswap	$0	$1
	ENDMACRO

	MACRO	movb	$0	$1
	mov	byteswap	$0	$1
	ENDMACRO

	MACRO	movwb	$0	$1
	mov	wordswap	byteswap	$0	$1
	ENDMACRO

	DEFINE	movbw	movwb

