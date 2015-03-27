#ifndef __VNODE_H__
#define __VNODE_H__

#include "vsys.h"
#include "vcfg.h"
#include "vroute.h"
#include "vupnpc.h"
#include "vnodeId.h"

struct vnice_res_status {
    int ratio;
    int criteria;
    int factor;
};

struct vnode_nice;
struct vnode_nice_ops {
    int  (*get_nice)(struct vnode_nice*);
    void (*dump)    (struct vnode_nice*);
};

struct vnode_nice {
    struct vnice_res_status cpu;
    struct vnice_res_status mem;
    struct vnice_res_status io;
    struct vnice_res_status net_up;   //network upload occupied status.
    struct vnice_res_status net_down;

    int prev_nice_val;
    struct vnode_nice_ops* ops;
};

int  vnode_nice_init  (struct vnode_nice*, struct vconfig*);
void vnode_nice_deinit(struct vnode_nice*);

/*
 * for vnode_addr_family
 */
#define VNODE_MAX_LOCAL_ADDRS ((int)3)
struct vnode_addr_helper {
    int32_t  naddrs;
    uint32_t mask;  //private address(local address) is unreflexed yet.
    struct sockaddr_in addrs[VNODE_MAX_LOCAL_ADDRS];
};

#define unreflexive_mask_check(mask, i)   (mask & (1 << i))
#define reflexive_mask_check(mask, i)     (!unreflexive_mask_check(mask, i))
#define unreflexive_mask_set(mask, i)     (mask |= (1 << i))
#define reflexive_mask_set(mask, i)       (mask &= ~(1 << i))
/*
 * for vnode
 */
struct vnode_ops {
    int  (*start)      (struct vnode*);
    int  (*stop)       (struct vnode*);
    int  (*wait_for_stop)(struct vnode*);
    int  (*stabilize)  (struct vnode*);
    int  (*reflex_addr)(struct vnode*, struct sockaddr_in*, struct sockaddr_in*);
    void (*dump)       (struct vnode*);
    void (*clear)      (struct vnode*);
    int  (*renice)     (struct vnode*);
    void (*tick)       (struct vnode*);
    int  (*is_self)    (struct vnode*, struct sockaddr_in*);
    int  (*post)       (struct vnode*, vsrvcHash*, struct sockaddr_in*);
    void (*unpost)     (struct vnode*, vsrvcHash*, struct sockaddr_in*);
};

struct vnode {
    int    tick_tmo;
    time_t ts;
    int    mode;
    struct vlock lock;  // for mode.

    int nice;
    struct varray services;
    struct vnode_addr_helper addr_helper;
    vnodeInfo_relax nodei;

    struct vnode_nice node_nice;
    struct vupnpc upnpc;

    struct vconfig* cfg;
    struct vticker* ticker;
    struct vroute*  route;

    struct vnode_ops* ops;
};

int  vnode_init  (struct vnode*, struct vconfig*, struct vhost*, vnodeId*);
void vnode_deinit(struct vnode*);

#endif
