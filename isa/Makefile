CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -O2

all: isagen

isagen: isagen.c
	$(CC) $(CFLAGS) -o $@ isagen.c

check: isagen
	./isagen -v meow.isa

clean:
	rm -f isagen

.PHONY: all check clean
