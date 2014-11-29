#ifndef __VLSCTL_H__
#define __VLSCTL_H__

#include "vrpc.h"
#include "vmsger.h"
#include "vhost.h"

enum {
    VLSCTL_HOST_UP,
    VLSCTL_HOST_DOWN,
    VLSCTL_HOST_EXIT,
    VLSCTL_HOST_DUMP,
    VLSCTL_DHT_QUERY,

    VLSCTL_NODE_JOIN,
    VLSCTL_NODE_DROP,

    VLSCTL_PLUG,
    VLSCTL_UNPLUG,
    VLSCTL_PLUGIN_REQ,

    VLSCTL_CFG_DUMP,
    VLSCTL_BUTT
};

struct vlsctl;
struct vlsctl_ops {
    int (*host_up)    (struct vlsctl*, void*, int);
    int (*host_down)  (struct vlsctl*, void*, int);
    int (*host_exit)  (struct vlsctl*, void*, int);
    int (*dump_host)  (struct vlsctl*, void*, int);
    int (*bogus_query)(struct vlsctl*, void*, int);
    int (*join_node)  (struct vlsctl*, void*, int);
    int (*drop_node)  (struct vlsctl*, void*, int);
    int (*srvc_pub)   (struct vlsctl*, void*, int);
    int (*srvc_unavai)(struct vlsctl*, void*, int);
    int (*srvc_prefer)(struct vlsctl*, void*, int);
    int (*dump_cfg)   (struct vlsctl*, void*, int);
    int (*dispatch)   (struct vlsctl*, void*, int);
};

struct vlsctl {
    struct vrpc   rpc;
    struct vmsger msger;
    struct vhost* host;

    struct vsockaddr addr;
    struct vlsctl_ops* ops;
};

int  vlsctl_init  (struct vlsctl*, struct vhost*);
void vlsctl_deinit(struct vlsctl*);

#endif

