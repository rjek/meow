; entry.s
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

		EXPORT	catflapVersion

		IMPORT	Sys_Reset
		IMPORT	syscallDispatch
		IMPORT	irqDispatch

		; main OS entry point - CPU starts executing here, as well
		; as it being our system call entry point.

syscall		CMP	sp, #0			; have we got a stack yet?
		BEQ	Sys_Reset		; nope, jump to reset
		B	syscallDispatch		; yes, jump to handler

		; we have (32 - 6) bytes to encode the version string here
		; uncomment once assembler can cope. :)
catflapVersion	DCB 	"Catflap 0.0 (17 Mar 2007)", #0

		ALIGN	32

irqhandler	B	irqDispatch
