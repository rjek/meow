	; MEOW Assembler standard library

	; Branch macros

	MACRO	beq	$0
	branch	eq	$0
	ENDMACRO
	
	MACRO	bne	$0
	branch	ne	$0
	ENDMACRO
	
	MACRO	bcs	$0
	branch	cs	$0
	ENDMACRO
	
	MACRO	bhs	$0
	branch	cs	$0
	ENDMACRO
	
	MACRO	bcc	$0
	branch	cc	$0
	ENDMACRO
	
	MACRO	blo	$0
	branch	cc	$0
	ENDMACRO
	
	MACRO	bmi	$0
	branch	mi	$0
	ENDMACRO
	
	MACRO	bpl	$0
	branch	pl	$0
	ENDMACRO
	
	MACRO	bvs	$0
	branch	vs	$0
	ENDMACRO
	DEFINE	berr	bvs
	
	MACRO	bvc	$0
	branch	vc	$0
	ENDMACRO
	DEFINE	bok	bvc
	
	MACRO	bhi	$0
	branch	hi	$0
	ENDMACRO
	
	MACRO	bls	$0
	branch	ls	$0
	ENDMACRO
	
	MACRO	bge	$0
	branch	ge	$0
	ENDMACRO
	
	MACRO	blt	$0
	branch	lt	$0
	ENDMACRO
	
	MACRO	bgt	$0
	branch	gt	$0
	ENDMACRO
	
	MACRO	ble	$0
	branch	le	$0
	ENDMACRO
	
	MACRO	bal	$0
	branch	al	$0
	ENDMACRO
	DEFINE	b	bal
	
	MACRO	bnv	$0
	branch	nv	$0
	ENDMACRO


	; Branch-Link

	
	MACRO	bleq	$0
	add	r14	pc	#4
	branch	eq	$0
	ENDMACRO
	
	MACRO	blne	$0
	add	r14	pc	#4
	branch	ne	$0
	ENDMACRO
	
	MACRO	blcs	$0
	add	r14	pc	#4
	branch	cs	$0
	ENDMACRO
	
	MACRO	blhs	$0
	add	r14	pc	#4
	branch	cs	$0
	ENDMACRO
	
	MACRO	blcc	$0
	add	r14	pc	#4
	branch	cc	$0
	ENDMACRO
	
	MACRO	bllo	$0
	add	r14	pc	#4
	branch	cc	$0
	ENDMACRO
	
	MACRO	blmi	$0
	add	r14	pc	#4
	branch	mi	$0
	ENDMACRO
	
	MACRO	blpl	$0
	add	r14	pc	#4
	branch	pl	$0
	ENDMACRO
	
	MACRO	blvs	$0
	add	r14	pc	#4
	branch	vs	$0
	ENDMACRO
	DEFINE	berr	bvs
	
	MACRO	blvc	$0
	add	r14	pc	#4
	branch	vc	$0
	ENDMACRO
	DEFINE	bok	bvc
	
	MACRO	blhi	$0
	add	r14	pc	#4
	branch	hi	$0
	ENDMACRO
	
	MACRO	blls	$0
	add	r14	pc	#4
	branch	ls	$0
	ENDMACRO
	
	MACRO	blge	$0
	add	r14	pc	#4
	branch	ge	$0
	ENDMACRO
	
	MACRO	bllt	$0
	add	r14	pc	#4
	branch	lt	$0
	ENDMACRO
	
	MACRO	blgt	$0
	add	r14	pc	#4
	branch	gt	$0
	ENDMACRO
	
	MACRO	blle	$0
	add	r14	pc	#4
	branch	le	$0
	ENDMACRO
	
	MACRO	blal	$0
	add	r14	pc	#4
	branch	al	$0
	ENDMACRO
	DEFINE	bl	blal
	
	MACRO	blnv	$0
	add	r14	pc	#4
	branch	nv	$0
	ENDMACRO

