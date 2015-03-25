#ifndef __VRTOUTE_H__
#define __VRTOUTE_H__

#include "vnodeId.h"
#include "varray.h"
#include "vhost.h"
#include "vcfg.h"
#include "vsys.h"
#include "vdef.h"

#define NBUCKETS    (VTOKEN_BITLEN)

enum {
    PROP_UNREACHABLE          = 0x0,
    PROP_PING                 = (uint32_t)(1 << VDHT_PING),
    PROP_PING_R               = (uint32_t)(1 << VDHT_PING_R),
    PROP_FIND_NODE            = (uint32_t)(1 << VDHT_FIND_NODE),
    PROP_FIND_NODE_R          = (uint32_t)(1 << VDHT_FIND_NODE_R),
    PROP_FIND_CLOSEST_NODES   = (uint32_t)(1 << VDHT_FIND_CLOSEST_NODES),
    PROP_FIND_CLOSEST_NODES_R = (uint32_t)(1 << VDHT_FIND_CLOSEST_NODES_R),
    PROP_REFLEX               = (uint32_t)(1 << VDHT_REFLEX),
    PROP_REFLEX_R             = (uint32_t)(1 << VDHT_REFLEX_R),
    PROP_PROBE_CONNECTIVITY   = (uint32_t)(1 << VDHT_PROBE),
    PROP_PROBE_CONNECTIVITY_R = (uint32_t)(1 << VDHT_PROBE_R),
    PROP_POST_SERVICE         = (uint32_t)(1 << VDHT_POST_SERVICE),
    PROP_FIND_SERVICE         = (uint32_t)(1 << VDHT_FIND_SERVICE),
    PROP_FIND_SERVICE_R       = (uint32_t)(1 << VDHT_FIND_SERVICE_R)
};

#define PROP_DHT_MASK  ((uint32_t)0x000003ff)

char* vroute_srvc_get_desc(int);

/*
 * for record space
 */
struct vhost;
struct vroute;
struct vroute_recr_space;
struct vroute_recr_space_ops {
    int  (*make)         (struct vroute_recr_space*, vtoken*);
    int  (*check)        (struct vroute_recr_space*, vtoken*);
    void (*timed_reap)   (struct vroute_recr_space*);
    void (*clear)        (struct vroute_recr_space*);
    void (*dump)         (struct vroute_recr_space*);
};

struct vroute_recr_space {
    int max_recr_period;
    struct vlist records; //has all dht query(but not received rsp yet) records;
    struct vlock lock;

    struct vroute_recr_space_ops* ops;
};

int  vroute_recr_space_init  (struct vroute_recr_space*);
void vroute_recr_space_deinit(struct vroute_recr_space*);

/*
 * for node space
 */
struct vroute_node_space;
struct vroute_node_space_ops {
    int  (*add_node)     (struct vroute_node_space*, vnodeInfo*, int);
    int  (*get_node)     (struct vroute_node_space*, vnodeId*, vnodeInfo*);
    int  (*get_neighbors)(struct vroute_node_space*, vnodeId*, struct varray*, int);
    int  (*air_service)  (struct vroute_node_space*, void*);
    int  (*reflex_addr)  (struct vroute_node_space*, struct sockaddr_in*);
    int  (*adjust_connectivity)
                         (struct vroute_node_space*, vnodeId*, vnodeConn*);
    int  (*probe_connectivity)
                         (struct vroute_node_space*, struct sockaddr_in*);
    int  (*tick)         (struct vroute_node_space*);
    int  (*load)         (struct vroute_node_space*);
    int  (*store)        (struct vroute_node_space*);
    void (*clear)        (struct vroute_node_space*);
    void (*dump)         (struct vroute_node_space*);
};

struct vroute_node_space {
    vnodeId  myid;
    vnodeVer myver;
    struct sockaddr_in zaddr;
    char db[BUF_SZ];
    int  bucket_sz;
    int  max_snd_tms;
    int  max_rcv_tmo;

    struct vroute* route;
    struct vroute_node_space_bucket {
        struct varray peers;
        time_t ts;
    } bucket[NBUCKETS];
    struct vroute_node_space_ops* ops;
};

int  vroute_node_space_init  (struct vroute_node_space*, struct vroute*, struct vconfig*, vnodeId*);
void vroute_node_space_deinit(struct vroute_node_space*);

/*
 *
 */
struct vroute_srvc_space;
struct vroute_srvc_space_ops {
    int  (*add_service)  (struct vroute_srvc_space*, vsrvcInfo*);
    int  (*get_service)  (struct vroute_srvc_space*, vsrvcId*, vsrvcInfo**);
    void (*clear)        (struct vroute_srvc_space*);
    void (*dump)         (struct vroute_srvc_space*);
};

struct vroute_srvc_space {
    int bucket_sz;
    struct vroute_srvc_space_bucket {
        struct varray srvcs;
    } bucket[NBUCKETS];
    struct vroute_srvc_space_ops* ops;
};

int  vroute_srvc_space_init  (struct vroute_srvc_space*, struct vconfig*);
void vroute_srvc_space_deinit(struct vroute_srvc_space*);

/*
 * for routing table.
 */
struct vroute_ops {
    int  (*join_node)    (struct vroute*, struct sockaddr_in*);
    int  (*probe_service)(struct vroute*, vsrvcId*, vsrvcInfo_iterate_addr_t, void*);
    int  (*air_service)  (struct vroute*, vsrvcInfo*);
    int  (*reflex)       (struct vroute*, struct sockaddr_in*);
    int  (*probe_connectivity)
                         (struct vroute*, struct sockaddr_in*);
    int  (*load)         (struct vroute*);
    int  (*store)        (struct vroute*);
    int  (*tick)         (struct vroute*);
    void (*clear)        (struct vroute*);
    void (*dump)         (struct vroute*);
};

struct vroute_dht_ops {
    int (*ping)          (struct vroute*, vnodeConn*);
    int (*ping_rsp)      (struct vroute*, vnodeConn*, vtoken*, vnodeInfo*);
    int (*find_node)     (struct vroute*, vnodeConn*, vnodeId*);
    int (*find_node_rsp) (struct vroute*, vnodeConn*, vtoken*, vnodeInfo*);
    int (*find_closest_nodes)
                         (struct vroute*, vnodeConn*, vnodeId*);
    int (*find_closest_nodes_rsp)
                         (struct vroute*, vnodeConn*, vtoken*, struct varray*);
    int (*reflex)        (struct vroute*, vnodeConn*);
    int (*reflex_rsp)    (struct vroute*, vnodeConn*, vtoken*, struct sockaddr_in*);
    int (*probe)         (struct vroute*, vnodeConn*, vnodeId*);
    int (*probe_rsp)     (struct vroute*, vnodeConn*, vtoken*);
    int (*post_service)  (struct vroute*, vnodeConn*, vsrvcInfo*);
};

typedef int (*vroute_dht_cb_t)(struct vroute*, vnodeConn*, void*);
struct vroute {
    vnodeId  myid;
    uint32_t props;

    struct vroute_node_space node_space;
    struct vroute_srvc_space srvc_space;
    struct vroute_recr_space recr_space;

    struct vlock lock;

    struct vroute_ops*     ops;
    struct vroute_dht_ops* dht_ops;
    vroute_dht_cb_t*       cb_ops;
    struct vdht_enc_ops*   enc_ops;
    struct vdht_dec_ops*   dec_ops;

    struct vconfig* cfg;
    struct vmsger*  msger;
    struct vnode*   node;
};

int  vroute_init  (struct vroute*, struct vconfig*, struct vhost*, vnodeId*);
void vroute_deinit(struct vroute*);

#endif
