
; simple Chairman serial port test
; echos whatever is said

SysCall			B	main
			
			ALIGN	32
			
			IRQRTN

SerialFlags		DCD #0x2410
SerialRead		DCD #0x2414
SerialWrite		DCD #0x2418

main
			; r10 = base of memory
			EOR	r10, r10
			BIS	r10, #27
			
			; set stack pointer to be 1024 bytes past that
			MOV	sp, r10
			LDI	#1
			LSL	ir, #10
			ADD	sp, ir
			
			; r9 = current write pointer
			MOV	r9, r10
			
			; r8 = address of serial flags register
			LDI	#31		; chip select 31
			LSL	ir, #27
			MOV	r8, ir
			ADR	r0, SerialFlags
			LDR	r0, r0
			ADD	r8, r0
			
			; r7 = address of serial read register
			MOV	r7, r8
			ADD	r7, #4
			
			; r6 = address of serial write register
			MOV	r6, r8
			ADD	r6, #8

			ADR	r0, Greeting
			BL	WriteString

EchoLoop		BL	ReadChar
			STR	r0, r6		; write it out
			CMP	r0, #0
			BEQ	Exit
			B	EchoLoop

Exit			B	Exit			


Greeting		DCB	"Greetings.  Type stuff.", #10, #0
			ALIGN

WriteString		; r0 > pointer to string to write, null-terminated
			
			PUSH2	r0, r1
			
			EOR	r1, r1
WriteLoop		LDRBI	r1, r0
			CMP	r1, #0		; null terminator?
			BEQ	>WriteExit
			STR	r1, r6
			B	<WriteLoop

WriteExit		POP2	r1, r0
			MOV	pc, lr

ReadChar		; r0 < character read
			
			PUSH	r1
			
ReadCharLoop		LDR	r1, r8		; load status register
			TST	r1, #1		; fresh char?
			BEQ	ReadCharLoop	; no, check again
			LDR	r0, r7		; yes, load byte
			
			POP	r1
			
			MOV	pc, lr
