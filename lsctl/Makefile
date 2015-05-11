ROOT_PATH := ..

include ../common.mk

libvdhtapi := libvdhtapi.a

libvdhtapi_objs := \
        vhashgen.o \
        vlsctlc.o  \
        vdhtapi.o 

.PHONY: vlsctlc clean

vlsctlc: vlsctlc_main.o $(libvdhtapi)
	$(CC) -o $@ $^ $(LDFLAGS)
	$(RM) -f $<

$(libvdhtapi): $(libvdhtapi_objs)
	$(AR) -r $@ $^
	$(RM) -f $^

clean:
	-$(RM) -f $(libvdhtapi_objs)
	-$(RM) -f vlsctlc_main.o
	-$(RM) -f $(libvdhtapi)
	-$(RM) -f vlsctlc

