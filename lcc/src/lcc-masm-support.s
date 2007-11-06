	; MASM support header for lcc-generated code

	DEFINE	EXPORT	GLOBAL
	DEFINE	IMPORT	GLOBAL

	DEFINE	at	r10

	DEFMACRO	SEGMENT	$0
	; Do nothing with SEGMENT declarations for now
	ENDMACRO

	DEFMACRO	FUNCSTA	$0	$1
	; $0 is the function name
	; $1 is the function name as a string
	; Do nothing with function starts for now.
	ENDMACRO

	DEFMACRO	FUNCEND	$0	$1
	; $0 is the function name
	; $1 is the function name as a string
	; Do nothing with function ends for now.
	ENDMACRO

	DEFMACRO	STACKFRAME	$0
	; $0 is the number of bytes needed for the stack frame
	sub	sp	$0
	mov	fp	sp
	ENDMACRO

	DEFMACRO	ENDSTACKFRAME	$0
	; $0 is the number of bytes needed for the stack frame
	; Do nothing with stack frames fornow.
	add	sp	$0
	ENDMACRO

