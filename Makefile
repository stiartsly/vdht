CC=gcc
CFLAGS=-I. -Iutils -I/usr/local/include -g -Wall -std=gnu99 -D_STUB
LDFLAGS=-lpthread -lsqlite3 -lrt
DEPS = 

vdht_objs = \
    vcfg.o  \
    vhost.o \
    vnode.o \
    vrpc.o  \
    vmsger.o \
    vdht.o  \
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

