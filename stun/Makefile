CFLAGS = -I. -I.. -I../utils -g -Wall
stun_objs = \
    vstun.o 

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

libstun.a: $(stun_objs)
	ar -rv $@ $^
	@mv $@ ../.

clean:
	-rm -f *.o
	-rm -f libstun.a 


