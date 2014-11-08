#ifndef __VCFG_H__
#define __VCFG_H__

#define DEF_HOST_TICK_TMO      "5s"
#define DEF_NODE_TICK_INTERVAL "7s"

#define DEF_ROUTE_DB_FILE      "route.db"
#define DEF_LSCTL_UNIX_PATH    "/var/run/vdht/lsctl_socket"

struct vconfig;
struct vconfig_ops {
    int  (*parse)  (struct vconfig*, const char*);
    int  (*clear)  (struct vconfig*);
    void (*dump)   (struct vconfig*);
    int  (*get_int)(struct vconfig*, const char*, int*);
    int  (*get_str)(struct vconfig*, const char*, char*, int);
};

struct vconfig {
    struct vlist items;
    struct vconfig_ops* ops;
};

int  vconfig_init  (struct vconfig*);
void vconfig_deinit(struct vconfig*);

#endif

