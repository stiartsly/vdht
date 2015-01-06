#ifndef __VKICKER_H__
#define __VKICKER_H__

#include "vupnpc.h"
#include "vroute.h"
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

struct vspy_res_status {
    int ratio;
    int criteria;
    int factor;
};

struct vresource;
struct vresource_ops {
    int (*get_nice)(struct vresource*, int*);
};

struct vresource {
    struct vspy_res_status cpu;
    struct vspy_res_status mem;
    struct vspy_res_status io;
    struct vspy_res_status up;   // network upload
    struct vspy_res_status down; // network download

    struct vresource_ops* ops;
};

int  vresource_init  (struct vresource*, struct vconfig*);
void vresource_deinit(struct vresource*);

struct vkicker;
struct vkicker_ops {
    int (*kick_nice)      (struct vkicker*);
    int (*kick_upnp_addr) (struct vkicker*);
    int (*kick_ext_addr)  (struct vkicker*);
    int (*kick_relay_addr)(struct vkicker*);
    int (*kick_nat_type)  (struct vkicker*);
};

struct vkicker {
    struct vresource res;
    struct vupnpc upnpc;
//    struct vstunc stunc;

    struct vconfig* cfg;
    struct vroute*  route;

    struct vkicker_ops* ops;
};

int  vkicker_init  (struct vkicker*, struct vroute* route, struct vconfig*);
void vkicker_deinit(struct vkicker*);

#endif

