ROOT_PATH := ..
include ../common.mk

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
	$(RM) -f $^

clean:
	-$(RM) -f $(libutils_objs)
	-$(RM) -f $(libutils)


