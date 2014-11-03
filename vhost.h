#ifndef __VHOST_H__
#define __VHOST_H__

#include "vrpc.h"
#include "vnode.h"
#include "vmsger.h"
#include "vticker.h"
#include "vlsctl.h"

#define HOST_SZ ((int)128)

struct vhost;
struct vhost_ops {
    int (*start)    (struct vhost*);
    int (*stop)     (struct vhost*);
    int (*join)     (struct vhost*, struct sockaddr_in*);
    int (*drop)     (struct vhost*, struct sockaddr_in*);
    int (*stabilize)(struct vhost*);
    int (*loop)     (struct vhost*);
    int (*req_quit) (struct vhost*);
    int (*dump)     (struct vhost*);
};

struct vhost {
    char myname[HOST_SZ];
    int  myport;
    int  to_quit;

    struct vmsger  msger;
    struct vrpc    rpc;
    struct vwaiter waiter;
    struct vticker ticker;
    struct vnode   node;
    struct vlsctl  lsctl;

    struct vconfig*   cfg;
    struct vhost_ops* ops;
};

struct vhost* vhost_create(struct vconfig*, const char*, int);
void vhost_destroy(struct vhost*);

#endif
