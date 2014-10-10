CC=gcc
CFLAGS=-I. -Iutils -g -Wall -std=gnu99
LDFLAGS=-lpthread -lsqlite3
DEPS = 

vdht_objs = \
    vhost.o \
    vnode.o \
    vrpc.o  \
    vmsger.o \
    vroute.o \
    vticker.o \
    vnodeId.o 

.PHONY: vdhtd clean

%.o: %.c 
	@$(CC) -c -o $@ $< $(CFLAGS)

vdhtd: vmain.o libvdht.a libutils.a
	@$(CC) -o $@ $^ libutils.a  $(LDFLAGS)

libvdht.a: $(vdht_objs)
	@ar -rv $@ $^

libutils.a:
	@cd utils && make libutils.a

clean:
	@cd utils && make clean
	-rm -f *.o 
	-rm -f libvdht.a
	-rm -f libutils.a
	-rm -f vdhtd 

