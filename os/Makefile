MASM=MASM_INCLUDE=../assembler/lib MASM_PATH=../assembler/share ../assembler/masm

all: catflap.a

clean:
	rm -rf catflap.a catflap.s entry.o irq.o syscall.o init.o
	
catflap.a: entry.o irq.o syscall.o init.o
	$(MASM) -c -o catflap.s entry.o irq.s syscall.o init.o
	grep -v "^;" < catflap.s > catflap.a
	
entry.o: entry.s catflap.h
	$(MASM) -o entry.o -c entry.s

irq.o: irq.s catflap.h
	$(MASM) -o irq.o -c irq.s	

syscall.o: syscall.s catflap.h
	$(MASM) -o syscall.o -c syscall.s

init.o: init.s catflap.h
	$(MASM) -o init.o -c init.s
