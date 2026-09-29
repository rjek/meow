# The C library for MEOW: PDCLib with a platform layer for the machine and
# msim, and musl's maths.  Everything is compiled by nmcc in C99 mode into
# libc.a, an ar archive that mld reads.  See ../docs/libc.md.
NMCC = ../../norcroft-ng/bin/nmcc
MAS = ../as/mas
AR = ar
CFLAGS = -std=c99 -c
INC = -Ipdclib/include -Imeow/include

include sources.mk
MEOW_SRCS := $(wildcard meow/functions/*.c) $(COMMON_SRCS) $(FDLIBM_SRCS)
MEOW_ASM := $(wildcard meow/functions/*.s)
OBJS := $(patsubst %.c,obj/%.o,$(PDCLIB_SRCS) $(MEOW_SRCS) $(MUSL_SRCS)) \
        $(patsubst %.s,obj/%.o,$(MEOW_ASM))

# needs the compiler; without it the library is simply not built
all:
	@if [ -x $(NMCC) ]; then $(MAKE) libc.a; \
	else echo "libc.a not built: no compiler at $(NMCC)"; fi

# everything is compiled against the configuration
$(OBJS): meow/include/pdclib/_PDCLIB_config.h sources.mk

libc.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $(OBJS)

obj/musl/%.o: musl/%.c
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) $(MUSL_INC) $(INC) -o $@ $<

obj/fdlibm/%.o: fdlibm/%.c
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) $(MUSL_INC) $(INC) -o $@ $<

obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) $(INC) -o $@ $<

obj/%.o: %.s
	@mkdir -p $(dir $@)
	$(MAS) -f elf -o $@ $<

check: all
	cd ../tests && ./runlibc.sh

clean:
	rm -rf obj libc.a

.PHONY: all check clean
