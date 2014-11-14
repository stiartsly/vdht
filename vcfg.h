#ifndef __VCFG_H__
#define __VCFG_H__

#define DEF_HOST_TICK_TMO       "5s"
#define DEF_NODE_TICK_INTERVAL  "7s"

#define DEF_ROUTE_DB_FILE       "route.db"
#define DEF_ROUTE_BUCKET_CAPC   ((int)10)
#define DEF_ROUTE_MAX_SND_TIMES ((int)10)
#define DEF_LSCTL_UNIX_PATH     "/var/run/vdht/lsctl_socket"

#define DEF_DHT_PORT            ((int)12300)


struct vconfig;
struct vconfig_ops {
    int  (*parse)      (struct vconfig*, const char*);
    int  (*clear)      (struct vconfig*);
    void (*dump)       (struct vconfig*);
    int  (*get_int)    (struct vconfig*, const char*, int*);
    int  (*get_int_ext)(struct vconfig*, const char*, int*, int);
    int  (*get_str)    (struct vconfig*, const char*, char*, int);
    int  (*get_str_ext)(struct vconfig*, const char*, char*, int, char*);
};

struct vconfig {
    struct vlist items;
    struct vlock lock;

    struct vconfig_ops* ops;
};

int  vconfig_init  (struct vconfig*);
void vconfig_deinit(struct vconfig*);

#endif

