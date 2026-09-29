# Catflap, an operating system for MEOW.  The kernel is C compiled by
# nmcc with no C library; boot.s is linked first so that the vectors land
# at 0 and 32.  The ROM is the kernel, then the shared library's data
# relocation list, then a romfs holding the programs, which are linked
# against the kernel's symbols after the kernel is.
NMCC = ../../norcroft-ng/bin/nmcc
MAS = ../as/mas
MLD = ../ld/mld
MSIM = ../simulator/msim
HOSTCC = cc
CFLAGS = -std=c99 -c -I../libc/pdclib/include -I../libc/meow/include -Ikernel
RT = ../rt/mul.o ../rt/div.o ../rt/ll.o
RAM = 256

KSRCS := $(wildcard kernel/*.c)
KOBJS := obj/kernel/boot.o $(filter-out obj/kernel/init.o,$(patsubst %.c,obj/%.o,$(KSRCS))) \
         obj/kernel/libc_start.o obj/kernel/libc_end.o
LINK = $(KOBJS) obj/kernel/init.o $(RT)
PROGS := $(patsubst bin/%.c,obj/root/bin/%,$(wildcard bin/*.c))
ROOT := $(shell find root -type f)

all: catflap.rom

catflap.rom: obj/kernel.bin obj/librelocs.bin obj/romfs.img
	cat $^ > $@

obj/kernel.bin obj/librelocs.bin: $(LINK)
	$(MLD) -f bin -p -B -d 0x08000000 -M catflap.map -o obj/kernel.bin \
	    -R __libc_data_start,__libc_data_end,obj/librelocs.bin $(LINK)

obj/catflap.elf: $(LINK)
	$(MLD) -f elf -B -d 0x08000000 -o $@ $(LINK)

# programs: linked at 0 with their addresses listed for the loader, the
# kernel's symbols absolute
obj/root/bin/%: obj/bin/%.o obj/lib/crt0.o obj/catflap.elf
	@mkdir -p obj/root/bin
	$(MLD) -f cfx -S obj/catflap.elf -o $@ obj/lib/crt0.o $<

obj/romfs.img: obj/mkromfs $(ROOT) $(PROGS)
	cp -r root/. obj/root/
	obj/mkromfs $@ obj/root

obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(MAS) -o $@ $<

obj/mkromfs: tools/mkromfs.c
	@mkdir -p obj
	$(HOSTCC) -O -Wall -o $@ $<

run: catflap.rom
	$(MSIM) -q -r catflap.rom -m $(RAM)

check: obj/kernel.bin obj/romfs.img
	../tests/runos.sh

clean:
	rm -rf obj catflap.rom catflap.map

.PHONY: all run check clean
