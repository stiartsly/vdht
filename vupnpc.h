#ifndef __VUPNPC_H__
#define __VUPNPC_H__

struct vupnpc_status {
    int bytes_snd;
    int bytes_rcv;
    int pkts_snd;
    int pkts_rcv;
};

enum {
    VUPNPC_PROTO_TCP,
    VUPNPC_PROTO_UDP,
    VUPNPC_PROTO_BUTT
};

struct vupnpc;
struct vupnpc_ops {
    int (*map_port)  (struct vupnpc*, int, int, int, int, char*);
    int (*unmap_port)(struct vupnpc*, int, int);
    int (*get_status)(struct vupnpc*, struct vupnpc_status*);
    void (*dump_mapping_port)(struct vupnpc*);
};

struct vupnpc {
    struct vupnpc_ops* ops;
};

int  vupnpc_init  (struct vupnpc*);
void vupnpc_deinit(struct vupnpc*);

#endif

