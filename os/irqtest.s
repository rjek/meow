
; Simple test that prints a ! every 5000 instruction cycles, 200 times before
; asking msim to quit.  This approximates one million instructions.
; (approximates because instructions in the IRQ handler do not count towards
; this value.)

SysCall			B	main
			
			ALIGN	32
			
IrqHandle
			; R10 -> pointer to pending interrupts register
			; R9 -> value to write to above to clear timer irq
			; R8 -> counter value - subtract on each iteration,
			;       and quit msim when it reaches zero.
				
			BNV	#-6		; msim print char
			
			STR	R9, R10		; clear interrupt
			SUB	R8, #1		; reduce counter
			CMP	R8, #0		; done yet?
			BNE	IrqReturn	; nope
			
			LDI	#0
			BNV	#-2		; quit msim
			
IrqReturn		IRQRTN

main			
			; set up the contents of the irq handler's registers,
			; so it doesn't have to to speed stuff up
			
			ADR	R0, PendingAddr
			LDR	R0, R0
			MOV	AR10, R0	; address of pending reg
			
			LDI	#1
			LSL	IR, #31
			MOV	AR9, IR		; value to write to pending
			
			LDI	#200		; interrupts before exit
			MOV	AR8, IR
			
			LDI	#33		; ASCII for '!'
			MOV	AIR, IR

			; set up interrupt mask so we actually receive
			; the timer interrupts
			ADR	R0, CPU0Mask
			LDR	R0, R0
			LDI	#1
			LSL	IR, #31
			STR	IR, R0
			
			; set the programmable timer to fire every 500 cycles
			
			ADR	R0, TimerReloadAddr
			LDR	R0, R0
			LDI	#1000
			ADD	IR, IR
			ADD	IR, IR
			ADD	IR, IR
			ADD	IR, IR
			STR	IR, R0
			
InfLoop			B	InfLoop

PendingAddr		DCD	#0xf8002400
CPU0Mask		DCD	#0xf8002000			
TimerReloadAddr		DCD	#0xf8002408			
