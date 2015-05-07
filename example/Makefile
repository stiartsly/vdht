ROOT_PATH := $(shell pwd)/..

include ../Makefile.def

.PHONY: verser vclient clean

all: vserver vclient

vserver: vserver_main.o libvdhtapi.a
	$(CC) -o $@ $^ $(LDFLAGS)

vclient: vclient_main.o libvdhtapi.a
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	-$(RM) -f *.o 
	-$(RM) -f vserver
	-$(RM) -f vclient

