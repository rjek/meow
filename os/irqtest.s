


SysCall			B	main
			
			ALIGN	32
			
IrqHandle
			LDI	#33
			BNV	#-6
			
			ADR	R0, PendingAddr
			LDR	R0, R0
			LDR	R1, R0
			BIC	R1, #31
			STR	R1, R0
			
			IRQRTN

PendingAddr		DCD	#0xf8002400

main
			ADR	R0, TimerReloadAddr
			LDR	R0, R0
			LDI	#500
			STR	IR, R0
			
InfLoop			B	InfLoop
			
TimerReloadAddr		DCD	#0xf8002408			
