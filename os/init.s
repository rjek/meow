; init.s
; This file is part of Catflap, an OS for MEOW
;
; Copyright (C) 2007 - Rob Kendrick <rjek@rjek.com>
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to
; deal in the Software without restriction, including without limitation the
; rights to use, copy, modify, merge, publish, distribute, and/or sell copies
; of the Software, and to permit persons to whom the Software is furnished to
; do so, subject to the following conditions:
; 
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
; 
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
; FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
; IN THE SOFTWARE.

		INCLUDE	catflap.h

		EXPORT	Sys_Reset

CPU0IrqMask	DCD	#0xf8002000
		
Sys_Reset	; first off, mask off all interrupts - if we're being
		; reset instead of initialised, we don't want to be
		; interrupted, otherwise things get sticky.  Slight race, here.
		
		ADR	r1, CPU0IrqMask
		LDR	r1, r1
		STR	r0, r1		; writes it into the mask register
		
		; we need to find some RAM before we can start calling
		; subroutines to find and initialise memory, as we'll be
		; needing a stack.  The MEOW specification defines that
		; chip select 0 is ROM and chip select 1 is RAM.  Catflap
		; doesn't support more than one chip select's worth of RAM,
		; so we just use this for now.  We set the interrupt mode's
		; stack pointer to be 512 bytes at the end, and the user
		; mode's stack pointer to be 1024 bytes before that.
		
		EOR	r0, r0
		BIS	r0, #27	

		LDI	#1024		; for later

memloop		LDR	r1, r0
		MVN	r1, r1
		STR	r1, r0
		LDR	r2, r0
		CMP	r1, r2
		BNE	>memnomore	; that memory doesn't work
		ADD	r0, ir		; move on 1024 bytes

		TST	r0, #28		; have we overflowed into the next CS?
		BNE	memnomore	; yes - don't dance on this memory
		
		B	<memloop

memnomore	SUB	r0, ir		; step back 1kB
		ADD	r0, #4		; we're a full decending stack!
		MOV	asp, r0		; set up IRQ mode SP
		LDI	IRQStackSize	; size of IRQ mode stack
		SUB	r0, ir
		MOV	sp, r0		; set up user mode SP
		ADD	r0, ir
		SUB	r0, #4		; take account of FD-nature
		
		BIC	r0, #27		; clear chip select bit
		LDI	#1
		LSL	ir, #27
		STR	r0, ir		; store size of ram in first word
		
		; now we can start initialising hardware.  Iterate through
		; each of the System Controller's chip select info regions,
		; and see if we recognise any of the hardware.
		
		LDI	#31
		LSL	ir, #27
		MOV	r2, ir
		
deviceloop	LDR	r1, r2		; read device id
		
		
