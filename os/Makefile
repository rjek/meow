# Catflap, an operating system for MEOW.  The kernel is C compiled by
# nmcc with no C library; boot.s is linked first so that the vectors land
# at 0 and 32.
NMCC = ../../norcroft-ng/bin/nmcc
MAS = ../as/mas
MLD = ../ld/mld
MSIM = ../simulator/msim
CFLAGS = -std=c99 -c -I../libc/pdclib/include -I../libc/meow/include -Ikernel
HOSTCC = cc
RT = ../rt/mul.o ../rt/div.o ../rt/ll.o
RAM = 256

KSRCS := $(wildcard kernel/*.c)
KOBJS := obj/kernel/boot.o $(filter-out obj/kernel/init.o,$(patsubst %.c,obj/%.o,$(KSRCS))) obj/kernel/romfs_image.o
ROOT := $(shell find root -type f)

all: catflap.rom

catflap.rom: $(KOBJS) obj/kernel/init.o $(RT)
	$(MLD) -f bin -d 0x08000000 -M catflap.map -o $@ $(KOBJS) obj/kernel/init.o $(RT)

obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(MAS) -I obj -o $@ $<

obj/kernel/romfs_image.o: kernel/romfs_image.s obj/romfs.img

obj/romfs.img: obj/mkromfs $(ROOT)
	obj/mkromfs $@ root

obj/mkromfs: tools/mkromfs.c
	@mkdir -p obj
	$(HOSTCC) -O -Wall -o $@ $<

run: catflap.rom
	$(MSIM) -q -r catflap.rom -m $(RAM)

check: $(KOBJS)
	../tests/runos.sh

clean:
	rm -rf obj catflap.rom catflap.map

.PHONY: all run check clean
