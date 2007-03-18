; io.s
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
		
		EXPORT	initIO
		EXPORT	Sys_PutC
		EXPORT	Sys_GetC
		EXPORT	Sys_PutS
		EXPORT	Sys_GetS
		
initIO
		MOV	pc, lr
		
Sys_PutC	; -> r0 == ASCII character to write
		
		MOV	pc, lr

Sys_GetC	; <- r0 == read ASCII value

		MOV	pc, lr
		
Sys_PutS	; -> r0 == pointer to C string to write

		MOV	pc, lr
		
Sys_GetS	; -> r0 == pointer to buffer to read string into
		; -> r1 == length of buffer
		
		MOV	pc, lr
