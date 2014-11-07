#ifndef __VNODE_H__
#define __VNODE_H__

#include "vcfg.h"
#include "vroute.h"
#include "vnodeId.h"
#include "vticker.h"
#include "vmsger.h"
#include "vsys.h"

#define VDHT_TICK_INTERVAL ((int)10)

enum vnode_mode {
    VDHT_OFF,
    VDHT_UP,
    VDHT_RUN,
    VDHT_DOWN,
    VDHT_ERR,
    VDHT_M_BUTT
};

//typedef int (*vpeer_cb_t)(void*, vpeer*);

struct vnode;
struct vnode_ops {
    int (*start)    (struct vnode*);
    int (*stop)     (struct vnode*);
    int (*join)     (struct vnode*, struct sockaddr_in*);
    int (*drop)     (struct vnode*, struct sockaddr_in*);
    int (*stabilize)(struct vnode*);
    int (*dump)     (struct vnode*);
//   int (*get_peers)(struct vnode*, vnodeHash*, vpeer_cb_t, void*);
};

struct vnode {
    vnodeAddr ownId;
    struct vroute route;

    int    tick_interval;
    time_t ts;
    int    mode;
    struct vlock lock;  // for mode.

    struct vconfig* cfg;
    struct vmsger*  msger;
    struct vticker* ticker;

    struct vnode_ops* ops;
};

int  vnode_init  (struct vnode*, struct vconfig*, struct vmsger*, struct vticker*, struct sockaddr_in*);
void vnode_deinit(struct vnode*);

#endif
