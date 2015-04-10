#ifndef __VCFG_H__
#define __VCFG_H__

enum {
    CFG_DICT,
    CFG_LIST,
    CFG_TUPLE,
    CFG_STR,
    CFG_BUTT
};

struct vcfg_item {
    int type;
    int depth;
    union {
        struct vcfg_list {
            char* key;
            struct varray l;
        } l;
        struct varray t;
        struct vdict d;
        char* s;
    }val;
};

struct vconfig;
struct vconfig_ops {
    int  (*parse)(struct vconfig*, const char*);
    int  (*clear)(struct vconfig*);
    void (*dump) (struct vconfig*);
    int  (*check)(struct vconfig*, const char*);

    int            (*get_int_val)  (struct vconfig*, const char*);
    const char*    (*get_str_val)  (struct vconfig*, const char*);
    struct vdict*  (*get_dict_val) (struct vconfig*, const char*);
    struct varray* (*get_list_val) (struct vconfig*, const char*);
    struct varray* (*get_tuple_val)(struct vconfig*, const char*);
};

typedef int (*vcfg_load_boot_node_t)(struct sockaddr_in*, void*);
struct vconfig_ext_ops {
    int (*get_syslog_switch)       (struct vconfig*);
    const char* (*get_syslog_ident)(struct vconfig*);
    const char* (*get_lsctl_socket)(struct vconfig*);

    int (*get_boot_nodes)          (struct vconfig*, vcfg_load_boot_node_t, void*);
    int (*get_host_tick_tmo)       (struct vconfig*);

    const char* (*get_route_db_file)(struct vconfig*);
    int (*get_route_bucket_sz)     (struct vconfig*);
    int (*get_route_max_snd_tms)   (struct vconfig*);
    int (*get_route_max_rcv_tmo)   (struct vconfig*);

    int (*get_dht_port)            (struct vconfig*);


};

struct vconfig {
    struct vcfg_item dict;

    struct vconfig_ops* ops;
    struct vconfig_ext_ops* ext_ops;
};

int  vconfig_init  (struct vconfig*);
void vconfig_deinit(struct vconfig*);

#endif

