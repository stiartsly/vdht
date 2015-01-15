#ifndef __VNODE_H__
#define __VNODE_H__

#include "vcfg.h"
#include "vroute.h"
#include "vnodeId.h"
#include "vticker.h"
#include "vmsger.h"
#include "vsys.h"

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

    struct vconfig* cfg;
    struct vticker* ticker;
    struct vroute*  route;

    struct vnode_ops*     ops;
    struct vnode_svc_ops* svc_ops;
};

int  vnode_init  (struct vnode*, struct vconfig*, struct vhost*, vnodeInfo*);
void vnode_deinit(struct vnode*);

#endif
