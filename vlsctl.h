#ifndef __VLSCTL_H__
#define __VLSCTL_H__

#include "vrpc.h"
#include "vmsger.h"
#include "vhost.h"

enum {
    VLSCTL_DHT_UP,
    VLSCTL_DHT_DOWN,
    VLSCTL_DHT_EXIT,

    VLSCTL_ADD_NODE,
    VLSCTL_DEL_NODE,

    VLSCTL_RELAY_UP,
    VLSCTL_RELAY_DOWN,
    VLSCTL_STUN_UP,
    VLSCTL_STUN_DOWN,
    VLSCTL_VPN_UP,
    VLSCTL_VPN_DOWN,

    VLSCTL_PING,
    VLSCTL_FIND_NODE,
    VLSCTL_FIND_CLOSEST_NODES,

    VLSCTL_LOGOUT,
    VLSCTL_CFGOUT,
    VLSCTL_BUTT
};

struct vlsctl;
struct vlsctl_ops {
    int (*dht_up)    (struct vlsctl*, void*, int);
    int (*dht_down)  (struct vlsctl*, void*, int);
    int (*dht_exit)  (struct vlsctl*, void*, int);
    int (*add_node)  (struct vlsctl*, void*, int);
    int (*del_node)  (struct vlsctl*, void*, int);
    int (*relay_up)  (struct vlsctl*, void*, int);
    int (*relay_down)(struct vlsctl*, void*, int);
    int (*stun_up)   (struct vlsctl*, void*, int);
    int (*stun_down) (struct vlsctl*, void*, int);
    int (*vpn_up)    (struct vlsctl*, void*, int);
    int (*vpn_down)  (struct vlsctl*, void*, int);
    int (*ping)      (struct vlsctl*, void*, int);
    int (*find_node) (struct vlsctl*, void*, int);
    int (*find_closest_nodes)
                     (struct vlsctl*, void*, int);

    int (*log_stdout)(struct vlsctl*, void*, int);
    int (*cfg_stdout)(struct vlsctl*, void*, int);
};

struct vlsctl {
    struct vrpc     rpc;
    struct vmsger   msger;
    struct vhost*   host;

    struct vsockaddr addr;
    struct vlsctl_ops* ops;
};

int  vlsctl_init  (struct vlsctl*, struct vhost*);
void vlsctl_deinit(struct vlsctl*);

#endif

