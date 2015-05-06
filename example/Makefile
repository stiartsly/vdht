CC=gcc
CFLAGS=-I. -I..  -I../lsctl -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS= -pthread
DEPS = 

.PHONY: verser vclient clean

all: vserver vclient

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

vserver: vserver_main.o libvdhtapi.a
	$(CC) -o $@ $^ $(LDFLAGS)

vclient: vclient_main.o libvdhtapi.a
	$(CC) -o $@ $^ $(LDFLAGS)


clean:
	-rm -f *.o 
	-rm -f vserver

