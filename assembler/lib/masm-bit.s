	; MEOW Standard assembler library

	; Bit twiddling, fiddling, or otherwise messings with

	MACRO	and	$0	$1
	bit	and	$0	$1
	ENDMACRO

	MACRO	orr	$0	$1
	bit	orr	$0	$1
	ENDMACRO

	MACRO	eor	$0	$1
	bit	eor	$0	$1
	ENDMACRO

	MACRO	not	$0
	bit	not	$0	$0
	ENDMACRO

	MACRO	mvn	$0	$1
	bit	not	$0	$1
	ENDMACRO

	MACRO	bis	$0	$1
	orr	$0	$1
	ENDMACRO

	MACRO	bic	$0	$1
	bit	and	inverted	$0	$1
	ENDMACRO

	MACRO	tog	$0	$1
	bit	eor	$0	$1
	ENDMACRO

