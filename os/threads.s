; a simple test that demonstrates multi-threading - it creates two threads
; and runs them.  The first thread outputs "a" to the MSIM console, and the
; second outputs "b".

		B	main
		ALIGN	32

IrqHandle	; r0 -> thread A's context
		; r1 -> thread B's context
		; r2 -> current thread, 0 == A, 1 == B
		
		; swap the context pointers around
		EOR    r0, r1
		EOR    r1, r0
		EOR    r0, r1
		
		BL	ContextSwitch
		
		; now clear the irq bit
		LDI	#1
		LSL	ir, #31
		
		ADR	r3, PendingIrqAddr
		LDR	r3, r3
		STR	ir, r3
		
		; return to user mode
		IRQRTN
		
PendingIrqAddr	DCD	#0xf8002400

ContextSwitch	; r0 -> pointer to context to switch in
		; r1 -> pointer to memory where to store current context
		; this must only be called from interrupt mode
		
		MACRO	SWITCH	$0
		MOV	r2, $0
		STRIA	r2, r1
		LDRIA	r2, r0
		MOV	$0, r2
		ENDMACRO
		
		PUSH3	r0, r1, r2
		
		SWITCH	ar0
		SWITCH	ar1
		SWITCH	ar2		
		SWITCH	ar3		
		SWITCH	ar4		
		SWITCH	ar5		
		SWITCH	ar6		
		SWITCH	ar7		
		SWITCH	ar8		
		SWITCH	ar9		
		SWITCH	ar10		
		SWITCH	ar11		
		SWITCH	ar12		
		SWITCH	ar13		
		SWITCH	ar14		
		SWITCH	ar15		

		POP3	r2, r1, r0
		MOV	pc, lr
		
main		; main entry point - put aside some RAM for the thread contexts
		; set up interrupt controller, initialise threads, program
		; interrupt timer.
		
		LDI	#1
		LSL	ir, #27
		MOV	r2, ir		; r2 == base of memory

		; set up the stacks
		LDI	#1
		LSL	ir, #12		; ir == 4096
		MOV	sp, r2
		ADD	sp, ir		; user stack pointer is 4kB into RAM
		
		MOV	r3, sp
		ADD	r3, ir
		MOV	asp, r3		; irq stack pointer is 8kB into RAM
		
		; set up pointers to thread contexts
		LDI	#64		; number of bytes needed for a context
		MOV	r0, r2		; r0 == pointer to thread a's context
		MOV	r1, r2
		ADD	r1, ir		; r1 == pointer to thread b's context
		
		; set up the PCs in the contexts to point to their thread
		; entry points
		LDI	#60		; PC is stored 60 bytes into a ctx
		
		MOV	r3, r0
		ADD	r3, ir
		ADR	r4, ThreadA
		STR	r4, r3		; we don't strictly need to set up
					; this for Thread A, as we branch to
					; it at the end of this routine.
		
		MOV	r3, r1
		ADD	r3, ir
		ADR	r4, ThreadB
		STR	r4, r3
		
		; set up interrupt mode's registers with the details it'll
		; need.
		MOV	ar0, r0		; ar0 == thread A context
		MOV	ar1, r1		; ar1 == thread B context
		
		; configure interrupt controller such that we receive the
		; timer's IRQs.
		ADR	r3, CPU0IrqMask
		LDR	r3, r3
		LDI	#1
		LSL	ir, #31
		STR	ir, r3
		
		; configure programmable interrupt timer
		ADR	r3, TimerReloadAddr
		LDR	r3, r3
		LDI	#1000
		STR	ir, r3		; the counter will now start
		
		; jump to the first thread.  This'll get a bit longer to run
		; initially, as it'll have nearly 1000 cycles to execute
		; just it, where future interruptions occur every 1000 cycles
		; including the number of cycles consumed by the interrupt
		; handler.
		
		B	ThreadA

CPU0IrqMask	DCD	#0xf8002000
TimerReloadAddr	DCD	#0xf8002408

ThreadA		LDI	#97		; ASCII for 'a'
		BNV	#-6		; MSIM for 'print char in IR'
		B	ThreadA
		
ThreadB		LDI	#98		; ASCII for 'b'
		BNV	#-6		; MSIM for 'print char in IR'
		B	ThreadB
		
