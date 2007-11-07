	; MASM support header for lcc-generated code

	DEFINE	EXPORT	GLOBAL
	DEFINE	IMPORT	GLOBAL

	DEFINE	at	r10

	DEFMACRO	SEGMENT	$0
	; Do nothing with SEGMENT declarations for now
	ENDMACRO

	DEFMACRO	FUNCENTER	$0	$1
	; $0 is the function name
	; $1 is the function name as a string
	; Do nothing with function starts for now.
	ENDMACRO

	DEFMACRO	FUNCEXIT	$0	$1
	; $0 is the function name
	; $1 is the function name as a string
	; Do nothing with function ends for now.
	ENDMACRO
