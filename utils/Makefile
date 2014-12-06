CFLAGS = -I. -I.. -g -Wall
utils_objs = \
    varray.o \
    vhash.o \
    vmisc.o \
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


