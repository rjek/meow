	; MEOW Assembler standard library

	; Branch macros

	MACRO	beq	$0
	b	eq	$0
	ENDMACRO

	MACRO	bne	$0
	b	ne	$0
	ENDMACRO

	MACRO	blt	$0
	b	lt	$0
	ENDMACRO

	MACRO	ble	$0
	b	le	$0
	ENDMACRO

	MACRO	bgt	$0
	b	gt	$0
	ENDMACRO

	MACRO	bge	$0
	b	ge	$0
	ENDMACRO

	MACRO	ber	$0
	b	er	$0
	ENDMACRO

	MACRO	bok	$0
	b	ok	$0
	ENDMACRO

	; Branch-Link macros

	MACRO	bleq	$0
	add	r14	pc	#4
	b	eq	$0
	ENDMACRO

	MACRO	blne	$0
	add	r14	pc	#4
	b	ne	$0
	ENDMACRO

	MACRO	bllt	$0
	add	r14	pc	#4
	b	lt	$0
	ENDMACRO

	MACRO	blle	$0
	add	r14	pc	#4
	b	le	$0
	ENDMACRO

	MACRO	blgt	$0
	add	r14	pc	#4
	b	gt	$0
	ENDMACRO

	MACRO	blge	$0
	add	r14	pc	#4
	b	ge	$0
	ENDMACRO

	MACRO	bler	$0
	add	r14	pc	#4
	b	er	$0
	ENDMACRO

	MACRO	blok	$0
	add	r14	pc	#4
	b	ok	$0
	ENDMACRO

