# Lua 5.4.7 for MEOW: the stock sources, compiled with nmcc against libc,
# linked into lua.bin for msim.  make run starts it interactively.
NMCC = ../../norcroft-ng/bin/nmcc
MLD = ../ld/mld
MSIM = ../simulator/msim
CFLAGS = -std=c99 -c -I../libc/pdclib/include -I../libc/meow/include
RT = ../rt/crt0.o ../rt/mul.o ../rt/div.o ../rt/ll.o ../rt/softfp.o
LIBC = ../libc/libc.a
RAM = 1024

SRCS := $(wildcard *.c)
OBJS := $(patsubst %.c,obj/%.o,$(SRCS))

all: lua.bin

lua.bin: $(OBJS) $(RT) $(LIBC)
	$(MLD) -f bin -d 0x08000000 -M lua.map -o $@ $(RT) $(OBJS) $(LIBC)

obj/%.o: %.c
	@mkdir -p obj
	$(NMCC) $(CFLAGS) -o $@ $< > obj/$*.log 2>&1 || (cat obj/$*.log; false)

run: lua.bin
	$(MSIM) -q -r lua.bin -m $(RAM)

check: lua.bin
	../tests/runlua.sh

clean:
	rm -rf obj lua.bin lua.map

.PHONY: all run check clean
