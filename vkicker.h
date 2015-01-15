#ifndef __VKICKER_H__
#define __VKICKER_H__

#include "vupnpc.h"
#include "vnode.h"
#include "vcfg.h"

enum {
    VNAT_OPEN,                 // open internet
    VNAT_SYMMETRIC_FIREWALL,   // symmetric firewall
    VNAT_FULL_CONE,            // full cone NAT
    VNAT_RESTRICTED_CONE,      //restricted cone NAT
    VNAT_PORT_RESTRICTED_CONE,
    VNAT_SYMMETRIC,
    VNAT_BUTT
};

struct vkicker;
struct vkicker_ops {
    int (*kick_upnp_addr) (struct vkicker*);
    int (*kick_ext_addr)  (struct vkicker*);
    int (*kick_relay_addr)(struct vkicker*);
    int (*kick_nat_type)  (struct vkicker*);
};

struct vkicker {
    struct vupnpc upnpc;
//    struct vstunc stunc;

    struct vconfig* cfg;
    struct vnode*  node;

    struct vkicker_ops* ops;
};

int  vkicker_init  (struct vkicker*, struct vnode*, struct vconfig*);
void vkicker_deinit(struct vkicker*);

#endif

