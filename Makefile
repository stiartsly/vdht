CC=gcc
CFLAGS=-I. -Iutils -Istun -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=-lpthread -lsqlite3 -lrt
DEPS = 

vdht_objs = \
    vcfg.o  \
    vrpc.o  \
    vdht.o  \
    vspy.o  \
    vdht_core.o \
    vhost.o \
    vnode.o \
    vperf.o \
    vmsger.o  \
    vroute.o  \
    vupnpc.o \
    vroute_node.o  \
    vroute_srvc.o  \
    vlsctl.o  \
    vticker.o \
    vnodeId.o \

.PHONY: all vdhtd vlsctlc clean

%.o: %.c 
	@$(CC) -c -o $@ $< $(CFLAGS)

all: vdhtd vlsctlc 

vdhtd: vmain.o libvdht.a libutils.a libstun.a
	@$(CC) -o $@ $^ libutils.a  $(LDFLAGS)

vlsctlc:
	@cd lsctl && make vlsctlc

libvdht.a: $(vdht_objs)
	@ar -rv $@ $^

libutils.a:
	@cd utils && make libutils.a

libstun.a:
	@cd stun  && make libstun.a

clean:
	@cd lsctl && make clean
	@cd utils && make clean
	@cd stun  && make clean
	-rm -f *.o 
	-rm -f libvdht.a
	-rm -f libutils.a
	-rm -f libstun.a
	-rm -f vdhtd 

