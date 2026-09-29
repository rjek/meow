	; MEOW Standard assembler library

	MACRO ext $0
	BNV	$0
	ENDMACRO

	MACRO irqrtn
	ext	#4
	ENDMACRO
