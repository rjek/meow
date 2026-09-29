MAS = ../as/mas
NMCC = ../../norcroft-ng/bin/nmcc

OBJS = crt0.o exit.o mul.o div.o ll.o msim.o

all: $(OBJS) softfp

.SUFFIXES: .s .o
.s.o:
	$(MAS) -f elf -o $@ $<

$(OBJS): $(MAS)

$(MAS):
	$(MAKE) -C ../as

# the floating-point library is C, so it needs the compiler
softfp:
	@if [ -x $(NMCC) ]; then $(MAKE) NMCC=$(NMCC) softfp.o; \
	else echo "softfp.o not built: no compiler at $(NMCC)"; fi

softfp.o: softfp.c $(NMCC)
	$(NMCC) -c -o $@ softfp.c

check: all
	cd ../tests && ./runc.sh

clean:
	rm -f $(OBJS) softfp.o

.PHONY: all check clean softfp
