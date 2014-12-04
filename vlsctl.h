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
    int (*dispatch)(struct vlsctl*, struct vmsg_usr*);
};

typedef int (*vlsctl_cmd_t)(struct vlsctl*, void*, int);
struct vlsctl {
    struct vrpc   rpc;
    struct vmsger msger;
    struct vhost* host;

    struct vsockaddr   addr;
    struct vlsctl_ops* ops;
    vlsctl_cmd_t*      cmds;
};

int  vlsctl_init  (struct vlsctl*, struct vhost*, struct vconfig*);
void vlsctl_deinit(struct vlsctl*);

#endif

