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
		
		GLOBAL	_main
		
		MACRO	PUSH	$0
		SUB	SP, #2
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
		
		CMP	SP, #0		; have we got a stack yet?
		BEQ	>Sys_Os_Reset	; no stack - we must be resetting.
		
		PUSH	R1
		
		ADR	R1, jumptable
		LSL	IR, #2
		ADD	IR, R1
		LDRLI	R1, IR
		LDRHD	R1, IR
		
		MOV	IR, R1
		POP	R1
		
		MOV	PC, IR
		
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
		
		MOV	R1, R0
		MOV	SP, R1		; use top of RAM as stack pointer
		
		LSL	R1, #5
		LSR	R1, #5		; clear chip select - size now in R1
		
		LDI	#1
		LSL	IR, #27
		STRLI	R1, IR
		STRHD	R1, IR		; store RAM size in first word of RAM
		
		
		BL	>displaybanner
		BL	_main		; branch to imported main function
		MOV	IR, R0		; _main's return value
		BNV	#-2		; msim quit()
		
nextfindmem	LDI	#1024
		ADD	R0, IR
		B	<findmemloop

implementor0	DCB	"Flarn ", #0
implementorX	DCB	"Unknown", #0

model0.0	DCB	"MSIM", #0
model0.1	DCB	"MEOW1", #0
		
		ALIGN	2

displaybanner
		PUSH	LR
		
		MACRO	NEWLINE
		LDI	#10
		BNV	#-6
		ENDMACRO
		
		ADR	R0, >banner
		SYS	OS_Write0
		
		ADR	R0, >bannerCPU
		SYS	OS_Write0
		
		BNV	#0		; read CPU information
		MOV	R0, IR
		LSR	R0, #24		; get implementor
		CMP	R0, #0		; is it flarn?
		BEQ	>flarnCPU
		
		ADR	R0, <implementorX
		SYS	OS_Write0
		NEWLINE
		B	>displayRAM
		
flarnCPU	ADR	R0, <implementor0
		SYS	OS_Write0

		BNV	#0
		LSR	IR, #16
		CMP	R0, #0
		ADR	R0, model0.0
		BNE	>flarnModel
		ADR	R0, model0.1
flarnModel	SYS	OS_Write0

		NEWLINE
		
displayRAM
		ADR	R0, >bannerRAM1
		SYS	OS_Write0
		
		SYS	OS_ReadSysInfo	; R0 = memory size
		MOV	IR, R0
		LSR	IR, #10
		BNV	#-8		; print integer in IR
		ADR	R0, >bannerRAM2
		SYS	OS_Write0
		
		POP	LR
		MOV	PC, LR

banner		DCB "Catflap 0.0 - Copyright (c) 2007, Rob Kendrick", #10, #0

bannerCPU	DCB "CPU: ", #0

bannerRAM1	DCB "RAM: ", #0
bannerRAM2	DCB " kB", #10, #10, #0

		ALIGN 2

Sys_OS_ReadSysInfo
		LDI	#1
		LSL	IR, #27
		LDRLI	R0, IR
		LDRHD	R0, IR
		
		MOV	PC, LR
		
Sys_OS_WriteC	MOV	IR, R0
		BNV	#-6		; msim "write character"
		MOV	PC, LR
		
Sys_OS_Write0	PUSH	R0
		PUSH	LR
		
		EOR	IR, IR
		
loop		LDRBI	IR, R0
		CMP	IR, #0
		BEQ	>exit

		; write this byte out
		;SYS	OS_WriteC
		BNV	#-6		; msim "write character"
		
		B	<loop		
exit		
		POP	LR
		POP	R0
		MOV	PC, LR

_main		MOV	PC, LR
