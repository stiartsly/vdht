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

struct vconfig_ext_ops {
    int (*get_lsctl_unix_path)   (struct vconfig*, char*, int);

    int (*get_host_tick_tmo)     (struct vconfig*, int*);
    int (*get_node_tick_interval)(struct vconfig*, int*);

    int (*get_route_db_file)     (struct vconfig*, char*, int);
    int (*get_route_bucket_sz)   (struct vconfig*, int*);
    int (*get_route_max_snd_tms) (struct vconfig*, int*);
    int (*get_route_max_rcv_period)   (struct vconfig*, int*);

    int (*get_dht_port)          (struct vconfig*, int*);

    int (*get_stun_port)         (struct vconfig*, int*);
    int (*get_stun_server_name)  (struct vconfig*, char*, int);
};

struct vconfig {
    struct vcfg_item dict;

    struct vconfig_ops* ops;
    struct vconfig_ext_ops* ext_ops;
};

int  vconfig_init  (struct vconfig*);
void vconfig_deinit(struct vconfig*);

#endif

