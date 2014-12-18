CC = gcc
CFLAGS = -I. -I.. -I../utils -g -Wall
LDFLAGS = 
DEPS = 

stun_objs = \
    vstun_proto.o \
    vstun.o 


.PHONY: libstun.a clean

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

libstun.a: $(stun_objs)
	ar -rv $@ $^
	@cp $@ ../.

clean:
	-rm -f *.o
	-rm -f libstun.a 

