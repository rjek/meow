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
# programs reach their static data through the process's static base, as
# the library does, so that their code can run in place from ROM
PCFLAGS = -zsb -std=c99 -c -I../libc/pdclib/include -I../libc/meow/include -Iinclude
PLINK = -f cfx -d __user_data_base -S obj/catflap.elf
RT = ../rt/mul.o ../rt/div.o ../rt/ll.o ../rt/softfp.o
RAM = 1024

KSRCS := $(wildcard kernel/*.c)
KOBJS := obj/kernel/boot.o $(filter-out obj/kernel/init.o,$(patsubst %.c,obj/%.o,$(KSRCS)))

# The shared C library: PDCLib, musl's maths and the Catflap platform
# layer, all of it, compiled with -zsb so that its data is reached
# through each process's displacement, and linked between the two marker
# objects.  The flags are libc/Makefile's.
LIBC = ../libc
include $(LIBC)/sources.mk
LIBC_CFLAGS = -zsb -std=c99 -c -I$(LIBC)/pdclib/include -I$(LIBC)/meow/include -Iinclude
CATFLAP_SRCS := $(wildcard $(LIBC)/catflap/functions/*.c) $(COMMON_SRCS) $(FDLIBM_SRCS)
CATFLAP_ASM := $(wildcard $(LIBC)/catflap/functions/*.s)
LIBOBJS := $(patsubst $(LIBC)/%.c,obj/libc/%.o,$(PDCLIB_SRCS) $(MUSL_SRCS) $(CATFLAP_SRCS)) \
           $(patsubst $(LIBC)/%.s,obj/libc/%.o,$(CATFLAP_ASM))
LINK = $(KOBJS) obj/kernel/init.o $(RT) obj/kernel/libc_start.o $(LIBOBJS) obj/kernel/libc_end.o
# Each bin/NAME.c is a program; Lua, from ../lua, is an archive whose
# lua.o member holds main, and the linker takes what it needs from it.
PROGS := $(patsubst bin/%.c,obj/root/bin/%,$(wildcard bin/*.c)) obj/root/bin/lua
LUA_SRCS := $(wildcard ../lua/*.c)
LUA_OBJS := $(patsubst ../lua/%.c,obj/lua/%.o,$(LUA_SRCS))
LUA_STACK = 16384
ROOT := $(shell find root -type f)

all: catflap.rom obj/link.list

# what tests/runos.sh links a test kernel from: everything but init.o
obj/link.list: Makefile $(KSRCS) $(CATFLAP_SRCS)
	@mkdir -p obj
	echo $(filter-out obj/kernel/init.o,$(LINK)) > $@

catflap.rom: obj/kernel.bin obj/librelocs.bin obj/romfs.img
	cat $^ > $@

obj/kernel.bin obj/librelocs.bin: $(LINK)
	$(MLD) -f bin -p -B -d 0x08000000 -M catflap.map -o obj/kernel.bin \
	    -R __libc_data_start,__libc_data_end,obj/librelocs.bin $(LINK)

obj/catflap.elf: $(LINK)
	$(MLD) -f elf -B -d 0x08000000 -o $@ $(LINK)

# programs: linked at 0 with their addresses listed for the loader, the
# kernel's and the library's symbols absolute
obj/root/bin/%: obj/bin/%.o obj/lib/crt0.o obj/catflap.elf
	@mkdir -p obj/root/bin
	$(MLD) $(PLINK) -o $@ obj/lib/crt0.o $<

obj/bin/lua.a: $(LUA_OBJS)
	rm -f $@
	ar rc $@ $(LUA_OBJS)

obj/lua/%.o: ../lua/%.c Makefile
	@mkdir -p obj/lua
	$(NMCC) $(PCFLAGS) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/root/bin/lua: obj/bin/lua.a obj/lib/crt0.o obj/catflap.elf
	@mkdir -p obj/root/bin
	$(MLD) $(PLINK) -k $(LUA_STACK) -o $@ obj/lib/crt0.o obj/bin/lua.a

# the romfs follows the kernel and the library's relocation list in ROM,
# and its programs are prelinked to run where they land
obj/romfs.img: obj/mkromfs $(ROOT) $(PROGS) obj/kernel.bin obj/librelocs.bin
	cp -r root/. obj/root/
	obj/mkromfs -b $$(( $$(wc -c < obj/kernel.bin) + $$(wc -c < obj/librelocs.bin) )) $@ obj/root

obj/bin/%.o: bin/%.c include/catflap.h Makefile
	@mkdir -p $(dir $@)
	$(NMCC) $(PCFLAGS) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/%.o: %.c kernel/kernel.h
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(MAS) -o $@ $<

obj/libc/musl/%.o: $(LIBC)/musl/%.c
	@mkdir -p $(dir $@)
	$(NMCC) $(LIBC_CFLAGS) $(MUSL_INC) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/libc/fdlibm/%.o: $(LIBC)/fdlibm/%.c
	@mkdir -p $(dir $@)
	$(NMCC) $(LIBC_CFLAGS) $(MUSL_INC) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

$(LIBOBJS): $(LIBC)/meow/include/pdclib/_PDCLIB_config.h $(LIBC)/sources.mk

obj/libc/%.o: $(LIBC)/%.c
	@mkdir -p $(dir $@)
	$(NMCC) $(LIBC_CFLAGS) -o $@ $< > $(@:.o=.log) 2>&1 || (cat $(@:.o=.log); false)

obj/libc/%.o: $(LIBC)/%.s
	@mkdir -p $(dir $@)
	$(MAS) -o $@ $<

obj/mkromfs: tools/mkromfs.c
	@mkdir -p obj
	$(HOSTCC) -O -Wall -o $@ $<

run: catflap.rom
	$(MSIM) -q -r catflap.rom -m $(RAM) -H .

check: obj/kernel.bin obj/link.list obj/lib/crt0.o $(patsubst bin/%.c,obj/bin/%.o,$(wildcard bin/*.c)) obj/bin/lua.a
	../tests/runos.sh

clean:
	rm -rf obj catflap.rom catflap.map

.PHONY: all run check clean
