CFLAGS=-std=c99 -pedantic -Wall -g -I../lib
LIBMEOW=../lib/libmeow.a

LUA=lua5.1

all: msim

clean:
	rm -rf msim msim_core.o msim_chairman.o msim_debug.o msim_hostfs.o msim_ioc.o msim.o

msim: msim_core.o msim_chairman.o msim_debug.o msim_hostfs.o msim_ioc.o msim.o $(LIBMEOW)
	$(CC) -o msim msim_core.o msim_chairman.o msim_debug.o msim_hostfs.o msim_ioc.o msim.o $(LIBMEOW) -ledit -llua5.1

$(LIBMEOW):
	$(MAKE) -C ../lib

check: msim
	cd ../tests && ./runsim.sh

.PHONY: all clean check

msim_core.o: msim_core.c msim_core.h config.h
	$(CC) $(CFLAGS) -c msim_core.c -o msim_core.o

msim_chairman.o: msim_chairman.c msim_chairman.h
	$(CC) $(CFLAGS) -c msim_chairman.c -o msim_chairman.o

msim_ioc.o: msim_ioc.c msim_ioc.h msim_chairman.h msim_core.h
	$(CC) $(CFLAGS) -c msim_ioc.c -o msim_ioc.o

msim_debug.o: msim_debug.c msim_debug.h msim_core.h config.h msim_watchpoints.c
	$(CC) $(CFLAGS) -c msim_debug.c -o msim_debug.o

msim_watchpoints.c: msim_watchpoints.lua bin2c.lua
	$(LUA) bin2c.lua +msim_watchpoints.lua > msim_watchpoints.c

msim.o: msim.c config.h
	$(CC) $(CFLAGS) -c msim.c -o msim.o
