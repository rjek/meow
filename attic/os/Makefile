MASM=MASM_INCLUDE=../assembler/lib MASM_PATH=../assembler/share ../assembler/masm

all: catflap.a

clean:
	rm -rf catflap.a catflap.s entry.o irq.o syscall.o init.o threads.o malloc.o io.o main.o
	
catflap.a: entry.o irq.o syscall.o init.o threads.o malloc.o io.o main.o
	cat entry.o irq.o syscall.o init.o threads.o malloc.o io.o main.o > catflap.a
	$(MASM) -o catflap.rom catflap.a
	
entry.o: entry.s catflap.h
	$(MASM) -o - -c entry.s | grep -v "^;" > entry.o

irq.o: irq.s catflap.h
	$(MASM) -o - -c irq.s | grep -v "^;" > irq.o

syscall.o: syscall.s catflap.h
	$(MASM) -o - -c syscall.s | grep -v "^;" > syscall.o

init.o: init.s catflap.h
	$(MASM) -o - -c init.s | grep -v "^;" > init.o

threads.o: threads.s catflap.h
	$(MASM) -o - -c threads.s | grep -v "^;" > threads.o
	
malloc.o: malloc.s catflap.h
	$(MASM) -o - -c malloc.s | grep -v "^;" > malloc.o

io.o: io.s catflap.h
	$(MASM) -o - -c io.s | grep -v "^;" > io.o

main.o: main.s catflap.h
	$(MASM) -o - -c main.s | grep -v "^;" > main.o
