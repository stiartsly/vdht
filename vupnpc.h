#ifndef __VUPNPC_H__
#define __VUPNPC_H__

#include "vsys.h"

enum {
    UPNP_PROTO_UDP,
    UPNP_PROTO_TCP,
    UPNP_PROTO_BUTT
};

enum {
    UPNPC_RAW,
    UPNPC_READY,
    UPNPC_ACTIVE,
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
    int (*setup)   (struct vupnpc*);
    int (*shutdown)(struct vupnpc*);
    int (*map)     (struct vupnpc*, struct sockaddr_in*, int, struct sockaddr_in*);
    int (*unmap)   (struct vupnpc*, uint16_t, int);
    int (*status)  (struct vupnpc*, struct vupnpc_status*);
};

struct vupnpc {
    void* config;
    int  state;
    char lan_iaddr[32];

    struct vupnpc_ops* ops;
};

int  vupnpc_init  (struct vupnpc*);
void vupnpc_deinit(struct vupnpc*);

#endif

