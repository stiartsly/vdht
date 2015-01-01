#ifndef __VUPNPC_H__
#define __VUPNPC_H__

#include "vsys.h"

enum {
    UPNPC_PROTO_TCP,
    UPNPC_PROTO_UDP,
    UPNPC_PROTO_BUTT
};

enum {
    UPNPC_RAW,
    UPNPC_NOIGD,
    UPNPC_READY,
    UPNPC_ACTIVE,
    UPNPC_ERR,
    UPNPC_BUTT
};

struct vupnpc_status {
    int bytes_snd;
    int bytes_rcv;
    int pkts_snd;
    int pkts_rcv;
};

struct vupnpc;
struct vupnpc_ops {
    int (*setup)     (struct vupnpc*);
    int (*shutdown)  (struct vupnpc*);

    int (*map_port)  (struct vupnpc*, int, int, int, struct sockaddr_in*);
    int (*unmap_port)(struct vupnpc*, int, int);
    int (*get_status)(struct vupnpc*, struct vupnpc_status*);
    void (*dump_mapping_port)(struct vupnpc*);
};

struct vupnpc {
    void* config;
    int   state;

    struct vlock lock;
    struct vupnpc_ops* ops;
};

int  vupnpc_init  (struct vupnpc*);
void vupnpc_deinit(struct vupnpc*);

#endif

