CFLAGS = -I. -I.. -g -Wall -D_GNU_SOURCE
utils_objs = \
    vhashmap.o \
    varray.o \
    vmisc.o \
    vdict.o \
    vmem.o \
    vsys.o 

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

libutils.a: $(utils_objs)
	ar -rv $@ $^
	@mv $@ ../.

clean:
	-rm -f *.o
	-rm -f libutils.a 


