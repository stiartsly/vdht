#ifndef __VCOLLECT_H__
#define __VCOLLECT_H__

enum {
    VNAT_OPEN,                 // open internet
    VNAT_SYMMETRIC_FIREWALL,   // symmetric firewall
    VNAT_FULL_CONE,            // full cone NAT
    VNAT_RESTRICTED_CONE,      //restricted cone NAT
    VNAT_PORT_RESTRICTED_CONE,
    VNAT_SYMMETRIC,
    VNAT_BUTT
};

struct vspy;
struct vspy_ops {
    int (*get_nice)    (struct vspy*, int*);
    int (*get_nat_type)(struct vspy*, int*);
};

struct vspy_res_status {
    int ratio;
    int criteria;
    int factor;
};

struct vspy {
    struct vspy_res_status cpu;
    struct vspy_res_status mem;
    struct vspy_res_status io;
    struct vspy_res_status up;   // network upload
    struct vspy_res_status down; // network download

    struct vspy_ops* ops;
};

int  vspy_init  (struct vspy*, struct vconfig*);
void vspy_deinit(struct vspy*);

#endif


