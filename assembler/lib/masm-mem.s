	; MEOW Standard Assembler library

	; Memory operations (load/store)

	; Load

	MACRO	ldrb	$0	$1
	mem	load	byte	low	$0	$1
	ENDMACRO

	MACRO	ldrh	$0	$1
	mem	load	half	high	$0	$1
	ENDMACRO

	MACRO	ldrl	$0	$1
	mem	load	half	low	$0	$1
	ENDMACRO

	MACRO	ldrbi	$0	$1
	mem	load	byte	low	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	ldrbd	$0	$1
	mem	load	byte	low 	writeback	decrementing	$0	$1
	ENDMACRO

	MACRO	ldrhi	$0	$1
	mem	load	half	high	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	ldrhd	$0	$1
	mem	load	half	high	writeback	decrementing	$0	$1
	ENDMACRO

	MACRO	ldrli	$0	$1
	mem	load	half	low	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	ldrld	$0	$1
	mem	load	half	low	writeback	decrementing	$0	$1
	ENDMACRO

	; Store

	MACRO	strb	$0	$1
	mem	store	byte	low	$0	$1
	ENDMACRO

	MACRO	strh	$0	$1
	mem	store	half	high	$0	$1
	ENDMACRO

	MACRO	strl	$0	$1
	mem	store	half	low	$0	$1
	ENDMACRO

	MACRO	strbi	$0	$1
	mem	store	byte	low	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	strbd	$0	$1
	mem	store	byte	low 	writeback	decrementing	$0	$1
	ENDMACRO

	MACRO	strhi	$0	$1
	mem	store	half	high	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	strhd	$0	$1
	mem	store	half	high	writeback	decrementing	$0	$1
	ENDMACRO

	MACRO	strli	$0	$1
	mem	store	half	low	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	strld	$0	$1
	mem	store	half	low	writeback	decrementing	$0	$1
	ENDMACRO

