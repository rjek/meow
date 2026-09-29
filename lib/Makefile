CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -O2
ISAGEN = ../isa/isagen
ISA = ../isa/meow.isa

OBJS = meow_isa.o meow.o disasm.o

all: libmeow.a

libmeow.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

meow_isa.h: $(ISA) $(ISAGEN)
	$(ISAGEN) -h $@ $(ISA)

meow_isa.c: $(ISA) $(ISAGEN)
	$(ISAGEN) -c $@ $(ISA)

$(ISAGEN):
	$(MAKE) -C ../isa

meow_isa.o: meow_isa.c meow_isa.h
meow.o: meow.c meow.h meow_isa.h
disasm.o: disasm.c meow.h meow_isa.h

test_disasm: test_disasm.c libmeow.a
	$(CC) $(CFLAGS) -o $@ test_disasm.c libmeow.a

check: test_disasm
	./test_disasm

clean:
	rm -f $(OBJS) libmeow.a meow_isa.c meow_isa.h test_disasm

.PHONY: all check clean
