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

		MACRO	SYS	$0	; syscall corrupts IR and LR
		LDI	$0
		ADD	lr, pc, #4
		EOR	pc, pc
		ENDMACRO
		
		DEFINE	IMPORT			GLOBAL
		DEFINE	EXPORT			GLOBAL
		
		DEFINE	nbit			#31
		DEFINE	zbit			#30
		DEFINE	cbit			#29
		DEFINE	vbit			#28
				
		; stack sizes
		DEFINE	IRQStackSize		#512
		DEFINE	USRStackSize		#1024
		
		; OS limits
		DEFINE	MaxThreads		#16
		
		; vendor ids
		DEFINE	vendor_none		#-1
		DEFINE	vendor_flarn		#0
		
		DEFINE	device_none		#-1
		
		; flarn device ids
		DEFINE	device_flarn_rom	#0
		DEFINE	device_flarn_ram	#1
		DEFINE	device_flarn_chairman	#2
		DEFINE	device_flarn_ioc	#3
		
		; syscall numbers
		DEFINE	OS_Reset		#0
		DEFINE	OS_ReadSysInfo		#1
		DEFINE	OS_Malloc		#2
		DEFINE	OS_Free			#3
		DEFINE	OS_NewThread		#4
		DEFINE	OS_DestroyThread	#5
		
		DEFINE	OS_PutC			#6	; put char
		DEFINE	OS_GetC			#7	; get char
		DEFINE	OS_PutS			#8	; put C string
		DEFINE	OS_GetS			#9	; get C string
		
		DEFINE	OS_Last			#9
