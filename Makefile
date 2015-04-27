CC=gcc
CFLAGS=-I. -Iutils -Istun -I/usr/local/include -I3rdparties/miniupnpc -g -Wall -std=gnu99 -D_STUB
LDFLAGS=-lpthread -lsqlite3 -lrt -L3rdparties/miniupnpc -lminiupnpc
DEPS = 

vdht_objs = \
    vlog.o \
    vcfg.o  \
    vapp.o \
    vrpc.o  \
    vdht.o  \
    vdht_core.o \
    vhost.o \
    vnode.o \
    vnode_nice.o \
    vperf.o \
    vmsger.o  \
    vroute.o  \
    vupnpc.o \
    vroute_node.o  \
    vroute_srvc.o  \
    vroute_recr.o  \
    vroute_helper.o \
    vlsctl.o  \
    vticker.o \
    vnodeId.o \

.PHONY: all vdhtd vlsctlc clean

%.o: %.c 
	@$(CC) -c -o $@ $< $(CFLAGS)

all: vdhtd vlsctlc libvdhtapi.a

vdhtd: vmain.o libvdht.a 
	@$(CC) -o $@ $^ libutils.a  $(LDFLAGS)

vlsctlc:
	@cd lsctl && make vlsctlc

libvdht.a: $(vdht_objs) libutils.a
	@ar -rv $@ $^

libutils.a:
	@cd utils && make libutils.a

libvdhtapi.a:
	@cd lsctl && make libvdhtapi.a

clean:
	@cd lsctl && make clean
	@cd utils && make clean
	-rm -f *.o 
	-rm -f libvdht.a
	-rm -f libutils.a
	-rm -f vdhtd 

