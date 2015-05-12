CC := gcc
AR := ar
RM := rm
MV := mv
CP := cp
MK := make
SD := sed

LIB_PATH := $(ROOT_PATH)/.lib

objects      := $(subst .c, .o, $(sources))
dependencies := $(subst .c, .d, $(sources))
include_dirs := $(ROOT_PATH) \
		$(ROOT_PATH)/utils \
		$(ROOT_PATH)/lsctl \
		$(ROOT_PATH)/3rdparties/miniupnpc

library_dirs := 3rdparties/miniupnpc 

CFLAGS  := -g -Wall -std=gnu99 -D_GNU_SOURCE $(addprefix -I , $(include_dirs))
LDFLAGS := -lpthread -lsqlite3 -lrt $(addprefix -L, $(library_dirs)) -lminiupnpc

$(libraries): $(objects)
	$(AR) -r $@ @^

#(%.o): %.c 
#	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $*.o
#	$(AR) -r $@ $*.o
#	$(RM) $*.o

%.d: %c
	$(CC) $(CFLAGS) -M $< | \
	$(SD) 's, \($*\.o) *:, \1 $@: ,' > $@.tmp
	$(MV)  $@.tmp $@

