CC=gcc
CFLAGS=-I. -I..  -Iutils -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=
DEPS = 

.PHONY: vlsctlc clean

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

vlsctlc: vlsctlc_main.o vlsctlc.o vhashgen.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	-rm -f *.o 
	-rm -f vlsctlc

