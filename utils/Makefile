ROOT_PATH := $(shell pwd)/..
include ../Makefile.def

libutils := libutils.a

libutils_objs := \
        vhashmap.o \
        varray.o \
        vmisc.o \
        vdict.o \
        vmem.o \
        vsys.o 

$(libutils): $(libutils_objs)
	$(AR) -r $@ $^
	$(MV) $@ ../.

clean:
	-$(RM) -f $(libutils_objs)
	-$(RM) -f $(libutils)


