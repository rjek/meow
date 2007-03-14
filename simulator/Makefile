CFLAGS=-std=c99 -pedantic -Wall -g

all: msim

clean:
	rm -rf msim msim_core.o msim_debug.o msim.o

msim: msim_core.o msim_debug.o msim.o
	$(CC) -o msim msim_core.o msim_debug.o msim.o -ledit

msim_core.o: msim_core.c msim_core.h config.h
	$(CC) $(CFLAGS) -c msim_core.c -o msim_core.o

msim_debug.o: msim_debug.c msim_debug.h msim_core.h config.h
	$(CC) $(CFLAGS) -c msim_debug.c -o msim_debug.o

msim.o: msim.c config.h
	$(CC) $(CFLAGS) -c msim.c -o msim.o

