all: msim

clean:
	rm -rf msim

msim: msim.c msim.h
	gcc -std=c99 -pedantic -Wall -DTEST_RIG -g -o msim msim.c
