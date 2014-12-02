#ifndef __VHOST_H__
#define __VHOST_H__

#include "vrpc.h"
#include "vnode.h"
#include "vmsger.h"
#include "vticker.h"
#include "vlsctl.h"
#include "vcollect.h"

struct vhost;
struct vhost_ops {
    int   (*start)      (struct vhost*);
    int   (*stop)       (struct vhost*);
    int   (*join)       (struct vhost*, struct sockaddr_in*);
    int   (*drop)       (struct vhost*, struct sockaddr_in*);
    int   (*stabilize)  (struct vhost*);
    int   (*plug)       (struct vhost*, int, struct sockaddr_in*);
    int   (*unplug)     (struct vhost*, int, struct sockaddr_in*);
    int   (*get_service)(struct vhost*, int, struct sockaddr_in*);
    int   (*loop)       (struct vhost*);
    int   (*req_quit)   (struct vhost*);
    void  (*dump)       (struct vhost*);
    char* (*get_version)(struct vhost*);
    int   (*bogus_query)(struct vhost*, int, struct sockaddr_in*);
};

struct vhost {
    vnodeAddr ownId;
    int  to_quit;
    int  tick_tmo;

    struct vmsger   msger;
    struct vrpc     rpc;
    struct vwaiter  waiter;
    struct vticker  ticker;
    struct vroute   route;
    struct vnode    node;
    struct vlsctl   lsctl;
    struct vcollect collect;

    struct vconfig*   cfg;
    struct vhost_ops* ops;
};

struct vhost* vhost_create(struct vconfig*);
void vhost_destroy(struct vhost*);

#endif
