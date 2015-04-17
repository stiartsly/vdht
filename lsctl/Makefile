CC=gcc
CFLAGS=-I. -I..  -Iutils -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=
DEPS = 

vdhtapi_objs = \
    vhashgen.o \
    vdhtapi.o 

.PHONY: vlsctlc clean

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

vdhtapi.a: $(vdhtapi_objs)
	ar -rv $@ $^

vlsctlc: vlsctlc_main.o vlsctlc.o vhashgen.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	-rm -f *.o 
	-rm -f vlsctlc

