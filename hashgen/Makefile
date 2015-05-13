ROOT_PATH := ..

include ../common.mk

bin_hashgen_objs := vhashgen_main.o
bin_hashgen      := vhashgen

objs := $(bin_hashgen_objs)
libs := 
apps := $(bin_hashgen) 

.PHONY: $(apps) $(libs) all clean
all: $(apps) $(libs)

$(bin_hashgen): $(bin_hashgen_objs)
	$(CC) -o $@ $^ $(LDFLAGS) 
	$(RM) -f $(bin_hashgen_objs)

clean:
	$(RM) -f $(objs)
	$(RM) -f $(libs)
	$(RM) -f $(apps)

