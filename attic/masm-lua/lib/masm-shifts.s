	; MEOW Standard assembler library

	; Shifts of various nefarious forms

	MACRO	lsl	$0	$1
	shift	left	$0	$1
	ENDMACRO

	MACRO	lsr	$0	$1
	shift	right	$0	$1
	ENDMACRO

	MACRO	asl	$0	$1
	shift	left	arithmetic	$0	$1
	ENDMACRO

	MACRO	asr	$0	$1
	shift	right	arithmetic	$0	$1
	ENDMACRO

	MACRO	rol	$0	$1
	shift	left	roll	$0	$1
	ENDMACRO

	MACRO	ror	$0	$1
	shift	right	roll	$0	$1
	ENDMACRO

	MACRO	arl	$0	$1
	shift	left	roll	arithmetic	$0	$1
	ENDMACRO

	MACRO	arr	$0	$1
	shift	right	roll	arithmetic	$0	$1
	ENDMACRO


