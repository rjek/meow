MAS = ../as/mas

OBJS = crt0.o mul.o div.o ll.o msim.o

all: $(OBJS)

.SUFFIXES: .s .o
.s.o:
	$(MAS) -f elf -o $@ $<

$(OBJS): $(MAS)

$(MAS):
	$(MAKE) -C ../as

check: all
	cd ../tests && ./runc.sh

clean:
	rm -f $(OBJS)

.PHONY: all check clean
