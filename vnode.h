#ifndef __VNODE_H__
#define __VNODE_H__

#include "vsys.h"
#include "vcfg.h"
#include "vstun.h"
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
 * for node upnp address/ external address/relayed address
 *
 */
struct vnode_addr;
struct vnode_addr_ops {
    int  (*setup)     (struct vnode_addr*);
    void (*shutdown)  (struct vnode_addr*);
    int  (*get_uaddr) (struct vnode_addr*, struct sockaddr_in*);
    int  (*get_eaddr) (struct vnode_addr*, get_ext_addr_t, void*);
    int  (*get_raddr) (struct vnode_addr*, struct sockaddr_in*);
};

struct vnode_addr {
    int iport;
    int eport;
    struct vupnpc upnpc;
    struct vstun  stun;

    struct vnode_addr_ops* ops;
};

int  vnode_addr_init  (struct vnode_addr*, struct vconfig*, struct vmsger*, struct vroute*, struct vhashgen*);
void vnode_addr_deinit(struct vnode_addr*);

/*
 * for vnode
 */
struct vnode;
struct vnode_svc_ops {
    int  (*registers) (struct vnode*, vsrvcId*, struct sockaddr_in*);
    void (*unregister)(struct vnode*, vsrvcId*, struct sockaddr_in*);
    void (*update)    (struct vnode*);
    void (*post)      (struct vnode*);
    void (*clear)     (struct vnode*);
    void (*dump)      (struct vnode*);
};

struct vnode_ops {
    int  (*start)     (struct vnode*);
    int  (*stop)      (struct vnode*);
    int  (*join)      (struct vnode*);
    int  (*stabilize) (struct vnode*);
    int  (*dump)      (struct vnode*);

    void (*get_own_node_info)(struct vnode*, vnodeInfo*);
    struct sockaddr_in* (*get_best_usable_addr)(struct vnode*, vnodeInfo*);
};

struct vnode {
    int    tick_interval;
    time_t ts;
    int    mode;
    struct vlock lock;  // for mode.

    vnodeInfo node_info;
    int nice;
    struct varray services;

    struct vnode_nice node_nice;
    struct vnode_addr node_addr;

    struct vconfig* cfg;
    struct vticker* ticker;
    struct vroute*  route;

    struct vnode_ops*     ops;
    struct vnode_svc_ops* svc_ops;
};

int  vnode_init  (struct vnode*, struct vconfig*, struct vhost*, vnodeInfo*);
void vnode_deinit(struct vnode*);

#endif
