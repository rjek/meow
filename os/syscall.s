; syscall.s
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
		
		EXPORT	syscallDispatch
		EXPORT	Sys_ReadSysInfo
		
		IMPORT	Sys_Reset

syscallUnknown	BIS	sp, vbit
		MOV	pc, lr		

syscallDispatch	CMP	ir, OS_Last
		BGT	<syscallUnknown
		PUSH	r1

		ADR	r1, syscallTable
		LSL	ir, #2
		ADD	ir, r1
		LDR	ir, ir
		
		POP	ir

		MOV	pc, ir

syscallTable
		DCD	Sys_Reset
		DCD	Sys_ReadSysInfo

Sys_ReadSysInfo
		EOR	r0, r0
		BIS	r0, #27
		LDR	r0, r0		; load memory size
		
		MOV	pc, lr
		
