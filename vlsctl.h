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

    VLSCTL_BUTT
};

struct vlsctl {
    struct vrpc     rpc;
    struct vmsger   msger;
    struct vhost*   host;

    struct vsockaddr addr;
};

int  vlsctl_init  (struct vlsctl*, struct vhost*);
void vlsctl_deinit(struct vlsctl*);

#endif

