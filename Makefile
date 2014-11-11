CC=gcc
CFLAGS=-I. -Iutils -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=-lpthread -lsqlite3 -lrt
DEPS = 

vdht_objs = \
    vcfg.o  \
    vrpc.o  \
    vdht.o  \
    vdht_core.o \
    vhost.o \
    vnode.o \
    vmsger.o  \
    vroute.o  \
    vlsctl.o  \
    vticker.o \
    vplugin.o \
    vnodeId.o 

.PHONY: all vdhtd vlsctlc clean

%.o: %.c 
	@$(CC) -c -o $@ $< $(CFLAGS)

all: vdhtd vlsctlc

vdhtd: vmain.o libvdht.a libutils.a
	@$(CC) -o $@ $^ libutils.a  $(LDFLAGS)

vlsctlc:
	@cd lsctl && make vlsctlc

libvdht.a: $(vdht_objs)
	@ar -rv $@ $^

libutils.a:
	@cd utils && make libutils.a

clean:
	@cd lsctl && make clean
	@cd utils && make clean
	-rm -f *.o 
	-rm -f libvdht.a
	-rm -f libutils.a
	-rm -f vdhtd 

