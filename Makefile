ROOT_PATH := $(shell pwd)

include Makefile.def

libutils   := libutils.a
libvdht    := libvdht.a 
libvdhtapi := libvdhtapi.a

vdhtd      := vdhtd
vlsctlc    := vlsctlc
vserver    := vserver
vclient    := vclient

libs       := $(libutils) $(libvdht) $(libvdhtapi)
apps       := $(vdhtd) $(vlsctlc) $(vserver) $(vclient)

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

.PHONY: $(apps) all clean

all: $(libs) $(apps)

$(libvdht): $(libvdht_objs) 
	$(AR) -r $@ $^ 

$(libutils):
	$(MK) --directory=utils $@

$(libvdhtapi):
	$(MK) --directory=lsctl $@

$(vdhtd): vmain.o $(libvdht) $(libutils)
	$(CC) -o $@ $^ $(LDFLAGS)

$(vlsctlc):
	$(MK) --directory=lsctl $@

$(vserver) $(vclient):
	$(MK) --directory=example $@

clean:
	$(MK) --directory=lsctl clean
	$(MK) --directory=utils clean
	$(MK) --directory=example clean
	$(RM) -f $(libvdht_objs)
	$(RM) -f vmain.o
	$(RM) -f $(libvdht)
	$(RM) -f $(libutils)
	$(RM) -f $(vdhtd)

