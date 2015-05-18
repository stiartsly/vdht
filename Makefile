ROOT_PATH := .
include common.mk

libvdht_objs  :=  vlog.o \
                  vcfg.o  \
                  vapp.o \
                  vrpc.o  \
                  vdht.o  \
                  vdht_core.o \
                  vhost.o \
        	  vnode.o \
      		  vnode_nice.o \
	          vmsger.o  \
	          vupnpc.o \
 	          vlsctl.o  \
                  vticker.o \
                  vnodeId.o \
	          vroute.o  \
                  vroute_node.o  \
	          vroute_srvc.o  \
                  vroute_recr.o  \
                  vroute_helper.o 

libvdht       := libvdht.a
libutils      := libutils.a
libvdhtapi    := libvdhtapi.a

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
	$(AR) -r $@ $(libvdht_objs) $(addprefix $(ROOT_PATH)/utils/, $(libutils))
	$(RM) -f $(libvdht_objs)

$(libutils):
	$(MK) --directory=utils $@

$(libvdhtapi):
	$(MK) --directory=lsctl $@

$(bin_vdhtd): $(bin_vdht_objs) $(libvdht)
	$(CC) -o $@ $^ $(addprefix $(ROOT_PATH)/utils/, $(libutils)) $(LDFLAGS)
	$(RM) -f $(bin_vdht_objs)

$(bin_lsctlc): $(libvdhtapi)
	$(MK) --directory=lsctl $@

$(bin_server) $(bin_client): $(libvdhtapi)
	$(MK) --directory=example $@

$(bin_hashgen): 
	$(MK) --directory=hashgen $@
clean:
	$(MK) --directory=lsctl clean
	$(MK) --directory=utils clean
	$(MK) --directory=example clean
	$(RM) -f $(objs)
	$(RM) -f $(libs)
	$(RM) -f $(apps)

