CUR_PATH  := .
ROOT_PATH := $(CUR_PATH)/..

include ../common.mk

libvdhtapi_incs := . ..
libvdhtapi_objs := vhashgen.o vlsctlc.o  vdhtapi.o 
libvdhtapi      := libvdhtapi.a

bin_lsctlc_objs := vlsctlc_main.o
bin_lsctlc_libs := $(libvdhtapi)
bin_lsctlc := vlsctlc

objs := $(libvdhtapi_objs) $(lsctlc_objs)
libs := $(libvdhtapi)
apps := $(lsctlc)

.PHONY: $(apps) $(libs) all clean
all: $(apps) $(libs)

$(bin_lsctlc): $(bin_lsctlc_objs) $(bin_lsctlc_libs)
	$(CC) -o $@ $^ $(LDFLAGS)
	$(RM) -f $<

$(libvdhtapi): $(libvdhtapi_objs)
	$(AR) -r $@ $^
	$(RM) -f $^

clean:
	$(RM) -f $(objs)
	$(RM) -f $(libs)
	$(RM) -f $(apps)

