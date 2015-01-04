#ifndef __VUPNPC_H__
#define __VUPNPC_H__

#include "vsys.h"

enum {
    UPNP_PROTO_TCP,
    UPNP_PROTO_UDP,
    UPNP_PROTO_BUTT
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
struct vupnpc_act_ops {
    int (*setup)            (struct vupnpc*);
    int (*shutdown)         (struct vupnpc*);
    int (*map_port)         (struct vupnpc*);
    int (*unmap_port)       (struct vupnpc*);
    int (*get_status)       (struct vupnpc*, struct vupnpc_status*);
    void (*dump_mapping_port)(struct vupnpc*);
};

struct vupnpc_ops {
    int (*start)            (struct vupnpc*);
    int (*stop)             (struct vupnpc*);
    int (*set_internal_port)(struct vupnpc*, uint16_t);
    int (*set_external_port)(struct vupnpc*, uint16_t);
    int (*get_internal_addr)(struct vupnpc*, struct sockaddr_in*);
    int (*get_external_addr)(struct vupnpc*, struct sockaddr_in*);
    int (*is_active)        (struct vupnpc*);
};

struct vupnpc {
    void* config;
    int state;
    int action;

    struct vlock   lock;
    struct vcond   cond;
    struct vthread thread;
    int to_quit;

    int proto;
    uint16_t iport;           //wanted intenral port
    uint16_t eport;           //wanted external port
    struct sockaddr_in iaddr;
    struct sockaddr_in eaddr;

    uint16_t old_iport;       //old internal port.
    uint16_t old_eport;       //old external port.


    struct vupnpc_ops* ops;
    struct vupnpc_act_ops* act_ops;
};

int  vupnpc_init  (struct vupnpc*);
void vupnpc_deinit(struct vupnpc*);

#endif

