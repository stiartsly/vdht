CC=gcc
CFLAGS=-I. -Iutils -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=-lpthread -lsqlite3 -lrt
DEPS = 

.PHONY: vlsctlc clean

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

vlsctlc: vlsctl_main.o 
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	-rm -f *.o 
	-rm -f vlsctlc

