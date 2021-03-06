ROOT_PATH := .
include common.mk

libvdht_objs  := vlog.o \
                 vcfg.o  \
                 vapp.o \
                 vrpc.o  \
                 vdht.o  \
                 vdht_core.o \
                 vhost.o \
                 vnode.o \
                 vnode_nice.o \
                 vmsger.o \
                 vupnpc.o \
                 vlsctl.o  \
                 vticker.o \
                 vnodeId.o \
                 vroute.o  \
                 vroute_node.o \
                 vroute_srvc.o \
                 vroute_tckt.o \
                 vdhtapp.o \

libvdht       := libvdht.so
libutils      := libutils.a
libvdhtapi    := libvdhtapi.so

bin_vdht_objs := vmain.o
bin_vdht_libs := $(libvdht) $(libutils)
bin_vdhtd     := vdhtd
bin_lsctlc    := vlsctlc
bin_server    := vserver
bin_client    := vclient
bin_hashgen   := vhashgen

objs          := $(libvdht_objs)
libs          := $(libutils) $(libvdht) $(libvdhtapi)
apps          := $(bin_vdhtd) $(bin_lsctlc) $(bin_server) $(bin_client)

.PHONY: $(apps) $(libs) all clean

all: $(libs) $(apps)

$(libvdht): $(libvdht_objs) $(libutils)
	$(CC) -fPIC -shared -o $@ $(libvdht_objs) $(addprefix $(ROOT_PATH)/utils/, $(libutils)) $(LDFLAGS)
	$(RM) -f $(libvdht_objs)

$(libutils):
	$(MK) --directory=utils $@

$(libvdhtapi):
	$(MK) --directory=lsctl $@

$(bin_vdhtd): $(bin_vdht_objs) $(libvdht)
	$(CC) -o $@ $^ $(LDFLAGS) -L. -lvdht
	$(RM) -f $(bin_vdht_objs)

$(bin_lsctlc): $(libvdhtapi)
	$(MK) --directory=lsctl $@

$(bin_server) $(bin_client): $(libvdhtapi)
	$(MK) --directory=example $@

$(bin_hashgen): 
	$(MK) --directory=hashgen $@

install:
	$(CP) -p $(libvdht)    /usr/local/lib
	$(CP) -p $(bin_vdhtd)  /usr/local/sbin
	$(CP) -p daemon/vdht   /etc/init.d/vdht 
	$(CP) -p daemon/vdht.conf /etc/vdht/vdht.conf
       
	$(MK) --directory=lsctl $@
clean:
	$(MK) --directory=lsctl clean
	$(MK) --directory=utils clean
	$(MK) --directory=example clean
	$(RM) -f $(objs)
	$(RM) -f $(libs)
	$(RM) -f $(apps)

