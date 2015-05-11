ROOT_PATH := ..
include ../common.mk

objs := $(subst .c,.o,$(notdir $(shell find . -name '*.c')))
lib  := libutils.a

.PHONY: $(lib)

$(lib): $(objs)
	$(AR) -r $@ $^
	$(RM) -f $^

clean:
	-$(RM) -f $(objs)
	-$(RM) -f $(lib)


