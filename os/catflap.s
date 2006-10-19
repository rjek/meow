; catflap.s
; This file is part of Catflap, a trivial OS for MEOW
;
; Copyright (C) 2006 - Rob Kendrick <rjek@rjek.com>
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

		MACRO	PUSH	$0
		SUB	R13, #2
		STRHD	$0, R13
		STRL	$0, R13
		ENDMACRO
		
		MACRO	POP	$0
		LDRLI	$0, R13
		LDRHI	$0, R13
		ENDMACRO

		MACRO	SYSCALL	$0
		PUSH	R0
		PUSH	R12
		PUSH	R14
		LDI	$0
		MOV	R0, R12
		LDI	#0
		ADD	R14, PC, #4
		MOV	PC, R12
		POP	R14
		POP	R12
		POP	R0
		ENDMACRO

syscall		; main OS entry point
		; r0 = 0 -> initialise OS
		; r0 = 1 -> get system information
		;           <- r0 = amount of RAM in machine
		; r0 = 2 -> write character in r1 to output
		; r0 = 3 -> write string pointed to by r1 to output
		
		ADR	R1, jumptable
		LSL	R0, #2
		ADD	R0, R1
		LDRLI	R1, R0
		LDRHD	R1, R0
		MOV	PC, R1
		
jumptable
		DCD	OS_Reset
		DCD	OS_ReadSysInfo
		DCD	OS_WriteC
		DCD	OS_Write0
		
		ALIGN 	32
		
irqhandle	; IRQ handler

		IRQRTN
		
OS_Reset	; first of all, find the amount of available RAM, and store
		; the number of bytes in the first word of RAM.  Memory is
		; assumed to be a multiple of 1kB.
		
		LDI	#1
		LSL	R12, #28
		MOV	R0, R12		; pointer to base of RAM in R0
		
		LDI	#-1
		MOV	R1, R12		; 0xffffffff test pattern in R1
		
findmemloop	STRLI	R1, R0
		STRHD	R1, R0		; store in next location
		
		LDRLI	R2, R0
		LDRHD	R2, R0		; load it back again
		
		CMP	R2, R1		; is it the same?
		BEQ	>nextfindmem	; yes - increase pointer and try next
		
		; the last memory store/load failed, so memory ends 1024 bytes
		; before the current value of R0.
		
		LDI	#1024
		MOV	R1, R0
		SUB	R1, R12
		MOV	R13, R1		; use top of RAM as stack pointer
		BIC	R1, #28		; clear chip select - size now in R1
		
		LDI	#1
		LSL	R12, #28
		STRLI	R1, R12
		STRHD	R1, R12		; store RAM size in first word of RAM
		
		
		B	>displaybanner				
		
nextfindmem	LDI	#1024
		ADD	R0, R12
		B	<findmemloop

displaybanner
		
		ADR	R1, >catflapbanner
		SYSCALL	#3

		BNV	#-128		; exit msim
		MOV	PC, R14

catflapbanner	DCB "Catflap - Copyright (c) 2006, Rob Kendrick", #10, #0
		ALIGN 2

OS_ReadSysInfo
		LDI	#1
		LSL	R12, #28
		LDRLI	R0, R12
		LDRHD	R0, R12
		
		MOV	PC, R14
		
OS_WriteC	MOV	R12, R0
		BNV	#-8		; msim "write character"
		MOV	PC, R14
		
OS_Write0	PUSH	R0
		PUSH	R1
		
loop		LDRBI	R1, R0
		CMP	R1, #0
		BEQ	>exit

		; write this byte out
		SYSCALL	#2
		
		B	<loop		
exit		
		POP	R1
		POP	R0
		MOV	PC, R14		

idleloop	B	idleloop
