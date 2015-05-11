ROOT_PATH := ..

include ../common.mk

objs := vhashgen.o vlsctlc.o  vdhtapi.o 
lib  := libvdhtapi.a
bin  := vlsctlc

.PHONY: $(bin) $(lib) clean

$(bin): vlsctlc_main.o $(lib)
	$(CC) -o $@ $^ $(LDFLAGS)
	$(RM) -f $<

$(lib): $(objs)
	$(AR) -r $@ $^
	$(RM) -f $^

clean:
	$(RM) -f $(objs) vlsctlc_main.o
	$(RM) -f $(lib)
	$(RM) -f $(bin)

