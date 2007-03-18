; catflap.h
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

		; useful macros
		DEFINE	IMPORT	GLOBAL
		DEFINE	EXPORT	GLOBAL
		
		DEFINE	nbit	#31
		DEFINE	zbit	#30
		DEFINE	cbit	#29
		DEFINE	vbit	#28
		
		MACRO	SYS	$0	; syscall corrupts IR and LR
		LDI	$0
		ADD	lr, pc, #4
		EOR	pc, pc
		ENDMACRO
		
		; stack sizes
		DEFINE	IRQStackSize	#512
		DEFINE	USRStackSize	#1024
		
		; syscall numbers
		DEFINE	OS_Reset	#0
		DEFINE	OS_ReadSysInfo	#1
		
		DEFINE	OS_Last		#1
