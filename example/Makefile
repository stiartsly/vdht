ROOT_PATH := $(shell pwd)/..

include ../Makefile.def

.PHONY: verser vclient clean

all: vserver vclient

vserver: vserver_main.o 
	$(CC) -o $@ $^ libvdhtapi.a $(LDFLAGS)
	$(RM) -f vserver_main.o
 

vclient: vclient_main.o
	$(CC) -o $@ $^ libvdhtapi.a $(LDFLAGS)
	$(RM) -f vclient_main.o

clean:
	-$(RM) -f libvdhtapi.a
	-$(RM) -f vserver_main.o 
	-$(RM) -f vclient_main.o 
	-$(RM) -f vserver
	-$(RM) -f vclient

