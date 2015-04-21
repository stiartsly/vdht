CC=gcc
CFLAGS=-I. -I..  -Iutils -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=
DEPS = 

libvdhtapi_objs = \
    vhashgen.o \
    vlsctlc.o \
    vdhtapi.o 

.PHONY: vlsctlc clean

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

vlsctlc: vlsctlc_main.o libvdhtapi.a
	$(CC) -o $@ $^ $(LDFLAGS)

libvdhtapi.a: $(libvdhtapi_objs)
	ar -rv $@ $^

clean:
	-rm -f *.o 
	-rm -f *.a
	-rm -f vlsctlc

