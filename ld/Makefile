CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -O2
CPPFLAGS = -I../lib
LIBMEOW = ../lib/libmeow.a

all: mld

mld: mld.o $(LIBMEOW)
	$(CC) $(CFLAGS) -o $@ mld.o $(LIBMEOW)

mld.o: mld.c ../lib/melf.h

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(LIBMEOW):
	$(MAKE) -C ../lib

check: mld
	cd ../tests && ./runld.sh

clean:
	rm -f mld.o mld

.PHONY: all check clean
