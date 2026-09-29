CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -O2
CPPFLAGS = -I../lib
LIBMEOW = ../lib/libmeow.a

OBJS = main.o diag.o lex.o expr.o symbol.o instr.o parse.o output.o

all: mas mdis mobjdump

mas: $(OBJS) $(LIBMEOW)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBMEOW)

mdis: mdis.o $(LIBMEOW)
	$(CC) $(CFLAGS) -o $@ mdis.o $(LIBMEOW)

mdis.o: mdis.c ../lib/meow.h ../lib/meow_isa.h

mobjdump: mobjdump.o $(LIBMEOW)
	$(CC) $(CFLAGS) -o $@ mobjdump.o $(LIBMEOW)

mobjdump.o: mobjdump.c ../lib/meow.h ../lib/melf.h

$(OBJS): mas.h ../lib/meow.h ../lib/meow_isa.h
output.o: ../lib/melf.h

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LIBMEOW):
	$(MAKE) -C ../lib

check: mas
	cd ../tests && ./run.sh

clean:
	rm -f $(OBJS) mdis.o mobjdump.o mas mdis mobjdump

.PHONY: all check clean
