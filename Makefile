ROOT_PATH := $(shell pwd)

include Makefile.def

libvdht = libvdht.a 

libvdht_objs := \
        vlog.o \
        vcfg.o  \
        vapp.o \
        vrpc.o  \
        vdht.o  \
        vdht_core.o \
        vhost.o \
        vnode.o \
        vnode_nice.o \
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

all: vdhtd vlsctlc $(libvdht) libutils.a libvdhtapi.a vserver vclient

$(libvdht_objs): vglobal.h

$(libvdht): $(libvdht_objs) libutils.a
	$(AR) -r $@ $^

vdhtd: vmain.o $(libvdht)
	@$(CC) -o $@ $^ libutils.a  $(LDFLAGS)

vlsctlc:
	@cd lsctl && make -e vlsctlc 

libutils.a:
	@cd utils && make libutils.a

libvdhtapi.a:
	@cd lsctl && make libvdhtapi.a

vserver:
	@cd example && make vserver

vclient:
	@cd example && make vclient 

clean:
	@cd lsctl && make clean
	@cd utils && make clean
	-$(RM) -f $(libvdht_objs)
	-$(RM) -f vmain.o
	-$(RM) -f $(libvdht)
	-$(RM) -f libutils.a
	-$(RM) -f vdhtd 

