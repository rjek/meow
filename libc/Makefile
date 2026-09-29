# The C library for MEOW: PDCLib with a platform layer for the machine and
# msim, and musl's maths.  Everything is compiled by nmcc in C99 mode into
# libc.a, an ar archive that mld reads.  See ../docs/libc.md.
NMCC = ../../norcroft-ng/bin/nmcc
MAS = ../as/mas
AR = ar
# TZ_MAX_TIMES sizes the time zone tables: 2000 transitions is 15 KB each
CFLAGS = -std=c99 -c -DTZ_MAX_TIMES=200
INC = -Ipdclib/include -Imeow/include
# musl's linkage attributes mean nothing here; its two weak aliases are
# done by hand in meow/functions/lgamma_r.c
MUSL_INC = -Dhidden= -D'weak_alias(o,n)=extern int __musl_no_weak_alias' \
           -Imeow/musl -Imusl/src/internal $(INC)

# remove() upstream wants a POSIX unlink; meow/functions/remove.c goes
# through the platform hook instead
PDCLIB_SRCS := $(filter-out pdclib/functions/stdio/remove.c,$(wildcard pdclib/functions/*/*.c))
MEOW_SRCS := $(wildcard meow/functions/*.c)
MEOW_ASM := $(wildcard meow/functions/*.s)
# PDCLib has these itself, in every precision
MUSL_DUP := fabs fabsf fabsl fdim fdimf fdiml fmax fmaxf fmaxl fmin fminf fminl
MUSL_SRCS := $(filter-out $(addprefix musl/src/math/,$(addsuffix .c,$(MUSL_DUP))),\
                          $(wildcard musl/src/math/*.c))
OBJS := $(patsubst %.c,obj/%.o,$(PDCLIB_SRCS) $(MEOW_SRCS) $(MUSL_SRCS)) \
        $(patsubst %.s,obj/%.o,$(MEOW_ASM))

# needs the compiler; without it the library is simply not built
all:
	@if [ -x $(NMCC) ]; then $(MAKE) libc.a; \
	else echo "libc.a not built: no compiler at $(NMCC)"; fi

libc.a: $(OBJS)
	rm -f $@
	$(AR) rcs $@ $(OBJS)

# dlmalloc: a heap grown by sbrk() and nothing from the host
obj/pdclib/functions/_dlmalloc/malloc.o: CFLAGS += -DHAVE_MMAP=0 -DHAVE_MORECORE=1 \
	-DLACKS_UNISTD_H -DLACKS_SYS_PARAM_H -DLACKS_SYS_MMAN_H \
	-DLACKS_SCHED_H -DLACKS_FCNTL_H -DLACKS_TIME_H -Dmalloc_getpagesize=4096

obj/musl/%.o: musl/%.c
	@mkdir -p $(dir $@)
	$(NMCC) $(CFLAGS) $(MUSL_INC) -o $@ $<

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
