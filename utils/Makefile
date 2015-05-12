CUR_PATH  := .
ROOT_PATH := $(CUR_PATH)/..

include ../common.mk

objs := $(subst .c,.o,$(notdir $(shell find . -name '*.c')))
lib  := libutils.a

libutils_incs := $(CUR_PATH) $(ROOT_PATH)
libutils_objs := $(subst .c,.o,$(notdir $(shell find . -name '*.c')))
libutils      := libutils.a

objs := $(libutils_objs)
libs := $(libutils)
apps := 

.PHONY: $(apps) $(libs) all clean
all: $(apps) $(libs)

$(libutils): $(libutils_objs)
	$(AR) -r $@ $^
	$(RM) -f $^

clean:
	$(RM) -f $(objs)
	$(RM) -f $(libs)
	$(RM) -f $(apps)


