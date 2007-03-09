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

		DEFINE	OS_Reset	#0
		DEFINE	OS_ReadSysInfo	#1
		DEFINE	OS_WriteC	#2
		DEFINE	OS_Write0	#3
		
		MACRO	PUSH	$0
		SUB	R13, #2
		STRHD	$0, SP
		STRL	$0, SP
		ENDMACRO
		
		MACRO	POP	$0
		LDRLI	$0, SP
		LDRHI	$0, SP
		ENDMACRO

		MACRO	SYS $0		; syscall - corrupts IR and LR
		LDI	$0
		EOR	LR, LR		; clear link register
		ADD	LR, PC, #4	; point to after our call
		EOR	PC, PC		; clear PC, branch to zero
		ENDMACRO


syscall		; main OS entry point
		; ir = 0 -> initialise OS
		; ir = 1 -> get system information
		;           <- r0 = amount of RAM in machine
		; ir = 2 -> write character in r0 to output
		; ir = 3 -> write string pointed to by r0 to output
		
		ADR	R1, jumptable
		LSL	IR, #2
		ADD	IR, R1
		LDRLI	R1, IR
		LDRHD	R1, IR
		MOV	PC, R1
		
jumptable
		DCD	Sys_OS_Reset
		DCD	Sys_OS_ReadSysInfo
		DCD	Sys_OS_WriteC
		DCD	Sys_OS_Write0
		
		ALIGN 	32
		
irqhandle	; IRQ handler

		IRQRTN
		
Sys_OS_Reset	; first of all, find the amount of available RAM, and store
		; the number of bytes in the first word of RAM.  Memory is
		; assumed to be a multiple of 1kB.
		
		LDI	#1
		LSL	IR, #27
		MOV	R0, IR		; pointer to base of RAM in R0
		
		LDI	#-1
		MOV	R1, IR		; 0xffffffff test pattern in R1
		
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
		SUB	R1, IR
		MOV	SP, R1		; use top of RAM as stack pointer
		BIC	R1, #27		; clear chip select - size now in R1
		
		LDI	#1
		LSL	IR, #27
		STRLI	R1, IR
		STRHD	R1, IR		; store RAM size in first word of RAM
		
		
		BL	>displaybanner
		B	idleloop
		
nextfindmem	LDI	#1024
		ADD	R0, IR
		B	<findmemloop

displaybanner
		
		ADR	R0, >catflapbanner
		SYS	OS_Write0
		MOV	PC, R14

catflapbanner	DCB "Catflap - Copyright (c) 2006, Rob Kendrick", #10, #0
		ALIGN 2

Sys_OS_ReadSysInfo
		LDI	#1
		LSL	IR, #27
		LDRLI	R0, IR
		LDRHD	R0, IR
		
		MOV	PC, R14
		
Sys_OS_WriteC	MOV	IR, R0
		BNV	#-6		; msim "write character"
		MOV	PC, R14
		
Sys_OS_Write0	PUSH	R0
		PUSH	R1
		
		MOV	R1, R0
loop		LDRBI	R0, R1
		CMP	R0, #0
		BEQ	>exit

		; write this byte out
		SYS	OS_WriteC
		
		B	<loop		
exit		
		POP	R1
		POP	R0
		MOV	PC, R14		

idleloop	B	idleloop
