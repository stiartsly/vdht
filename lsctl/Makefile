ROOT_PATH := $(shell pwd)/..

include ../Makefile.def

libvdhtapi := libvdhtapi.a

libvdhtapi_objs := \
        vhashgen.o \
        vlsctlc.o  \
        vdhtapi.o 

.PHONY: vlsctlc clean

vlsctlc: vlsctlc_main.o $(libvdhtapi)
	$(CC) -o $@ $^ $(LDFLAGS)

$(libvdhtapi): $(libvdhtapi_objs)
	$(AR) -r $@ $^

clean:
	-$(RM) -f $(libvdhtapi_objs)
	-$(RM) -f vlsctlc_main.o
	-$(RM) -f $(libvdhtapi)
	-$(RM) -f vlsctlc

