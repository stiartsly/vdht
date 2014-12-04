#ifndef __VCFG_H__
#define __VCFG_H__

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

struct vconfig_inst_ops {
    int (*get_lsctl_unix_path)   (struct vconfig*, char*, int);

    int (*get_host_tick_tmo)     (struct vconfig*, int*);
    int (*get_node_tick_interval)(struct vconfig*, int*);

    int (*get_route_db_file)     (struct vconfig*, char*, int);
    int (*get_route_bucket_sz)   (struct vconfig*, int*);
    int (*get_route_max_snd_tms) (struct vconfig*, int*);

    int (*get_dht_port)          (struct vconfig*, int*);

    int (*get_cpu_criteria)      (struct vconfig*, int*);
    int (*get_mem_criteria)      (struct vconfig*, int*);
    int (*get_io_criteria)       (struct vconfig*, int*);
    int (*get_up_criteria)       (struct vconfig*, int*);
    int (*get_down_criteria)     (struct vconfig*, int*);
    int (*get_cpu_factor)        (struct vconfig*, int*);
    int (*get_mem_factor)        (struct vconfig*, int*);
    int (*get_io_factor)         (struct vconfig*, int*);
    int (*get_up_factor)         (struct vconfig*, int*);
    int (*get_down_factor)       (struct vconfig*, int*);

    int (*get_ping_cap)          (struct vconfig*, int*);
    int (*get_ping_rsp_cap)      (struct vconfig*, int*);
    int (*get_find_node_cap)     (struct vconfig*, int*);
    int (*get_find_node_rsp_cap) (struct vconfig*, int*);
    int (*get_find_closest_nodes_cap)    (struct vconfig*, int*);
    int (*get_find_closest_nodes_rsp_cap)(struct vconfig*, int*);
    int (*get_post_service_cap)  (struct vconfig*, int*);
    int (*get_post_hash_cap)     (struct vconfig*, int*);
    int (*get_get_peers_cap)     (struct vconfig*, int*);
    int (*get_get_peers_rsp_cap) (struct vconfig*, int*);
};

struct vconfig {
    struct vlist items;
    struct vlock lock;

    struct vconfig_ops* ops;
    struct vconfig_inst_ops* inst_ops;
};

int  vconfig_init  (struct vconfig*);
void vconfig_deinit(struct vconfig*);

#endif

