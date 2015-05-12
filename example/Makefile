ROOT_PATH := ..

include ../common.mk

bin_server_objs := vserver_main.o
bin_server_libs := $(libvdhtapi)
bin_server      := vserver

bin_client_objs := vclient_main.o
bin_client_libs := $(libvdhtapi)
bin_client      := vclient

objs := $(bin_server_objs)
libs := 
apps := $(bin_server) $(bin_client)

.PHONY: $(apps) $(libs) all clean
all: $(apps) $(libs)

$(bin_server): $(bin_server_objs) $(bin_server_libs)
	$(CC) -o $@ $^ $(LDFLAGS) $(ROOT_PATH)/lsctl/libvdhtapi.a
	$(RM) -f $(bin_server_objs)
 
$(bin_client): $(bin_client_objs) $(bin_client_libs)
	$(CC) -o $@ $^ $(LDFLAGS) $(ROOT_PATH)/lsctl/libvdhtapi.a
	$(RM) -f $(bin_client_objs)

clean:
	$(RM) -f $(objs)
	$(RM) -f $(libs)
	$(RM) -f $(apps)

