	; MEOW Standard Assembler library

	; Memory operations (load/store)

	; Load

	MACRO	ldrb	$0	$1
	mem	load	byte	$0	$1
	ENDMACRO

	MACRO	ldrh	$0	$1
	mem	load	half	high	$0	$1
	ENDMACRO

	MACRO	ldrl	$0	$1
	mem	load	half	low	$0	$1
	ENDMACRO

	MACRO	ldrbi	$0	$1
	mem	load	byte	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	ldrbd	$0	$1
	mem	load	byte 	writeback	decrementing	$0	$1
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

	MACRO	ldr	$0	$1
	mem	load	word	$0 $1
	ENDMACRO

	MACRO	ldria	$0	$1
	mem	load	word	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	ldrib	$0	$1
	add	$1	#4
	ldr	$0	$1
	ENDMACRO

	MACRO	ldrda	$0	$1
	mem	load	word	writeback	decrementing	$0	$1
	ENDMACRO

	MACRO	ldrdb	$0	$1
	sub	$1	#4
	ldr	$0	$1
	ENDMACRO

	DEFINE	ldrfd	ldria

	MACRO	pop	$0
	ldrfd	$0	SP
	ENDMACRO
	
	MACRO	pop2	$0	$1
	pop	$0
	pop	$1
	ENDMACRO
	
	MACRO	pop3	$0	$1	$2
	pop	$0
	pop	$1
	pop	$2
	ENDMACRO
	
	MACRO	pop4	$0	$1	$2	$3
	pop	$0
	pop	$1
	pop	$2
	pop	$3
	ENDMACRO	

	MACRO	pop5	$0	$1	$2	$3	$4
	pop	$0
	pop	$1
	pop	$2
	pop	$3
	pop	$4
	ENDMACRO
	
	MACRO	pop6	$0	$1	$2	$3	$4	$5
	pop	$0
	pop	$1
	pop	$2
	pop	$3
	pop	$4
	pop	$5
	ENDMACRO
	
	MACRO	pop7	$0	$1	$2	$3	$4	$5	$6
	pop	$0
	pop	$1
	pop	$2
	pop	$3
	pop	$4
	pop	$5
	pop	$6
	ENDMACRO	
		
	; Store

	MACRO	strb	$0	$1
	mem	store	byte	$0	$1
	ENDMACRO

	MACRO	strh	$0	$1
	mem	store	half	high	$0	$1
	ENDMACRO

	MACRO	strl	$0	$1
	mem	store	half	low	$0	$1
	ENDMACRO

	MACRO	strbi	$0	$1
	mem	store	byte	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	strbd	$0	$1
	mem	store	byte 	writeback	decrementing	$0	$1
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

	MACRO	str	$0	$1
	mem	store	word	$0	$1
	ENDMACRO

	MACRO	stria	$0	$1
	mem	store	word	writeback	incrementing	$0	$1
	ENDMACRO

	MACRO	strib	$0	$1
	add	$1	#4
	str	$0	$1
	ENDMACRO

	MACRO	strda	$0	$1
	mem	store	word	writeback	decrementing	$0	$1
	ENDMACRO

	MACRO	strdb	$0	$1
	sub	$1	#4
	str	$0	$1
	ENDMACRO

	DEFINE	strfd	strdb

	MACRO	push	$0
	strfd	$0	SP
	ENDMACRO
	
	MACRO	push2	$0	$1
	sub	SP	#4
	strda	$0	SP
	str	$1	SP
	ENDMACRO

	MACRO	push3	$0	$1	$2
	sub	SP	#4
	strda	$0	SP
	strda	$1	SP
	str	$2	SP
	ENDMACRO

	MACRO	push4	$0	$1	$2	$3
	sub	SP	#4
	strda	$0	SP
	strda	$1	SP
	strda	$2	SP
	str	$3	SP
	ENDMACRO
	
	MACRO	push5	$0	$1	$2	$3	$4
	sub	SP	#4
	strda	$0	SP
	strda	$1	SP
	strda	$2	SP
	strda	$3	SP
	str	$4	SP
	ENDMACRO
	
	MACRO	push6	$0	$1	$2	$3	$4	$5
	sub	SP	#4
	strda	$0	SP
	strda	$1	SP
	strda	$2	SP
	strda	$3	SP
	strda	$4	SP
	str	$5	SP
	ENDMACRO

	MACRO	push7	$0	$1	$2	$3	$4	$5	$6
	sub	SP	#4
	strda	$0	SP
	strda	$1	SP
	strda	$2	SP
	strda	$3	SP
	strda	$4	SP
	strda	$5	SP
	str	$6	SP
	ENDMACRO
