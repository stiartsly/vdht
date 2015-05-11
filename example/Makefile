ROOT_PATH := ..

include ../common.mk

.PHONY: verser vclient clean

all: vserver vclient

vserver: vserver_main.o 
	$(CC) -o $@ $^ $(LDFLAGS) $(ROOT_PATH)/lsctl/libvdhtapi.a
	$(RM) -f vserver_main.o
 

vclient: vclient_main.o
	$(CC) -o $@ $^ $(LDFLAGS) $(ROOT_PATH)/lsctl/libvdhtapi.a
	$(RM) -f vclient_main.o

clean:
	-$(RM) -f vserver_main.o 
	-$(RM) -f vclient_main.o 
	-$(RM) -f vserver
	-$(RM) -f vclient

