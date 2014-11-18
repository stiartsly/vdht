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
    int (*host_up)   (struct vlsctl*, void*, int);
    int (*host_down) (struct vlsctl*, void*, int);
    int (*host_exit) (struct vlsctl*, void*, int);
    int (*host_dump) (struct vlsctl*, void*, int);
    int (*dht_query) (struct vlsctl*, void*, int);
    int (*add_node)  (struct vlsctl*, void*, int);
    int (*del_node)  (struct vlsctl*, void*, int);
    int (*plug)      (struct vlsctl*, void*, int);
    int (*unplug)    (struct vlsctl*, void*, int);
    int (*req_plugin)(struct vlsctl*, void*, int);
    int (*cfg_dump)  (struct vlsctl*, void*, int);
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

