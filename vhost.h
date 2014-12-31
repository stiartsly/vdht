#ifndef __VHOST_H__
#define __VHOST_H__

#include "vrpc.h"
#include "vspy.h"
#include "vnode.h"
#include "vlsctl.h"
#include "vmsger.h"
#include "vticker.h"
#include "vhashgen.h"
#include "stun/vstunc.h"

struct vhost;
struct vhost_ops {
    int   (*start)      (struct vhost*);
    int   (*stop)       (struct vhost*);
    int   (*join)       (struct vhost*, struct sockaddr_in*);
    int   (*drop)       (struct vhost*, struct sockaddr_in*);
    int   (*stabilize)  (struct vhost*);
    int   (*loop)       (struct vhost*);
    int   (*req_quit)   (struct vhost*);
    void  (*dump)       (struct vhost*);
    char* (*get_version)(struct vhost*);
    int   (*bogus_query)(struct vhost*, int, struct sockaddr_in*);
};

struct vhost_svc_ops {
    int (*publish)(struct vhost*, vtoken*, struct sockaddr_in*);
    int (*cancel) (struct vhost*, vtoken*, struct sockaddr_in*);
    int (*get)    (struct vhost*, vtoken*, struct sockaddr_in*);
};

struct vhost {
    vnodeInfo ownId;
    int  to_quit;
    int  tick_tmo;

    struct vmsger   msger;
    struct vrpc     rpc;
    struct vwaiter  waiter;
    struct vticker  ticker;
    struct vroute   route;
    struct vnode    node;
    struct vlsctl   lsctl;
    struct vspy     spy;
    struct vstunc   stunc;
    struct vhashgen hashgen;

    struct vconfig*   cfg;
    struct vhost_ops* ops;
    struct vhost_svc_ops* svc_ops;
};

struct vhost* vhost_create(struct vconfig*);
void vhost_destroy(struct vhost*);

#endif
