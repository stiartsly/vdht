#ifndef __VNODE_H__
#define __VNODE_H__

#include "vcfg.h"
#include "vroute.h"
#include "vnodeId.h"
#include "vticker.h"
#include "vmsger.h"
#include "vsys.h"

struct vnode;
struct vnode_ops {
    int (*start)    (struct vnode*);
    int (*stop)     (struct vnode*);
    int (*join)     (struct vnode*, struct sockaddr_in*);
    int (*drop)     (struct vnode*, struct sockaddr_in*);
    int (*stabilize)(struct vnode*);
    int (*dump)     (struct vnode*);
};

struct vnode {
    vnodeAddr ownId;

    int    tick_interval;
    time_t ts;
    int    mode;
    struct vlock lock;  // for mode.

    struct vconfig* cfg;
    struct vticker* ticker;
    struct vroute*  route;

    struct vnode_ops* ops;
};

int  vnode_init  (struct vnode*, struct vconfig*, struct vticker*, struct vroute*, vnodeAddr*);
void vnode_deinit(struct vnode*);

#endif
