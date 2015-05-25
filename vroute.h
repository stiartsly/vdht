#ifndef __VRTOUTE_H__
#define __VRTOUTE_H__

#include "vnodeId.h"
#include "varray.h"
#include "vhost.h"
#include "vcfg.h"
#include "vsys.h"
#include "vdef.h"
#include "vdht.h"

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
    struct varray records; //has all dht query(but not received rsp yet) records;

    struct vroute_recr_space_ops* ops;
};

int  vroute_recr_space_init  (struct vroute_recr_space*);
void vroute_recr_space_deinit(struct vroute_recr_space*);

/*
 * for connectivity.
 */
struct vroute_conn_space;
struct vroute_conn_space_ops {
    int  (*make)         (struct vroute_conn_space*, vtoken*, vnodeId*, struct vnodeConn*);
    int  (*adjust)       (struct vroute_conn_space*, vtoken*);
    void (*timed_reap)   (struct vroute_conn_space*);
    void (*clear)        (struct vroute_conn_space*);
    void (*dump)         (struct vroute_conn_space*);
};

struct vroute_conn_space {
    int max_tmo;
    struct varray items;

    struct vroute* route;
    struct vroute_conn_space_ops* ops;
};

int  vroute_conn_space_init  (struct vroute_conn_space*, struct vroute*);
void vroute_conn_space_deinit(struct vroute_conn_space*);

/*
 * for node space
 */
struct vpeer {
    vnodeConn  conn;
    vnodeInfo* nodei;
    time_t snd_ts;
    time_t rcv_ts;
    int ntries;
    int nprobes;
};

typedef void (*vroute_node_space_inspect_t)(struct vpeer*, void*, vtoken*, uint32_t);
struct vroute_node_space;
struct vroute_node_space_ops {
    int  (*add_node)     (struct vroute_node_space*, vnodeInfo*, int);
    int  (*get_node)     (struct vroute_node_space*, vnodeId*, vnodeInfo*);
    int  (*get_neighbors)(struct vroute_node_space*, vnodeId*, struct varray*, int);
    int  (*find_node)    (struct vroute_node_space*, vnodeId*);
    int  (*air_service)  (struct vroute_node_space*, vsrvcInfo*);
    int  (*probe_service)(struct vroute_node_space*, vsrvcHash*);
    int  (*reflex_addr)  (struct vroute_node_space*, struct sockaddr_in*);
    int  (*adjust_connectivity)
                         (struct vroute_node_space*, vnodeId*, vnodeConn*);
    int  (*probe_connectivity)
                         (struct vroute_node_space*, struct sockaddr_in*);
    int  (*tick)         (struct vroute_node_space*);
    int  (*load)         (struct vroute_node_space*);
    int  (*store)        (struct vroute_node_space*);
    void (*clear)        (struct vroute_node_space*);
    void (*inspect)      (struct vroute_node_space*, vroute_node_space_inspect_t, void*, vtoken*, uint32_t);
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
    int  max_probe_tms;

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
struct vservice {
    vsrvcInfo* srvci;
    time_t rcv_ts;
};

typedef void (*vroute_srvc_space_inspect_t)(struct vservice*, void*, vtoken*, uint32_t);
struct vroute_srvc_space;
struct vroute_srvc_space_ops {
    int  (*add_service)  (struct vroute_srvc_space*, vsrvcInfo*);
    int  (*get_service)  (struct vroute_srvc_space*, vsrvcHash*, vsrvcInfo*);
    void (*clear)        (struct vroute_srvc_space*);
    void (*inspect)      (struct vroute_srvc_space*, vroute_srvc_space_inspect_t, void*, vtoken*, uint32_t);
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
 *
 */
struct vroute_srvc_probe_space;
struct vroute_srvc_probe_space_ops {
    int  (*add_cb)       (struct vroute_srvc_probe_space*, vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);
    int  (*invoke)       (struct vroute_srvc_probe_space*, vsrvcHash*, vsrvcInfo*);
    void (*timed_reap)   (struct vroute_srvc_probe_space*);
    void (*clear)        (struct vroute_srvc_probe_space*);
    void (*dump)         (struct vroute_srvc_probe_space*);
};

struct vroute_srvc_probe_space {
    int max_tmo;
    struct varray items;
    struct vroute_srvc_probe_space_ops* ops;
};

int  vroute_srvc_probe_space_init  (struct vroute_srvc_probe_space*);
void vroute_srvc_probe_space_deinit(struct vroute_srvc_probe_space*);

/*
 * for inspection
 */
enum {
    VROUTE_INSP_RESERVE   = (uint32_t)0x0,
    VROUTE_INSP_SND_PING  = (uint32_t)0x01,
    VROUTE_INSP_SND_PING_RSP,
    VROUTE_INSP_SND_FIND_NODE,
    VROUTE_INSP_SND_FIND_NODE_RSP,
    VROUTE_INSP_SND_FIND_CLOSEST_NODES,
    VROUTE_INSP_SND_FIND_CLOSEST_NODES_RSP,
    VROUTE_INSP_SND_REFLEX,
    VROUTE_INSP_SND_REFLEX_RSP,
    VROUTE_INSP_SND_PROBE,
    VROUTE_INSP_SND_PROBE_RSP,
    VROUTE_INSP_SND_POST_SERVICE,
    VROUTE_INSP_SND_FIND_SERVICE,
    VROUTE_INSP_SND_FIND_SERVICE_RSP,
    VROUTE_INSP_RCV_PING  = (uint32_t)0x81,
    VROUTE_INSP_RCV_PING_RSP,
    VROUTE_INSP_RCV_FIND_NODE,
    VROUTE_INSP_RCV_FIND_NODE_RSP,
    VROUTE_INSP_RCV_FIND_CLOSEST_NODES,
    VROUTE_INSP_RCV_FIND_CLOSEST_NODES_RSP,
    VROUTE_INSP_RCV_REFLEX,
    VROUTE_INSP_RCV_REFLEX_RSP,
    VROUTE_INSP_RCV_PROBE,
    VROUTE_INSP_RCV_PROBE_RSP,
    VROUTE_INSP_RCV_POST_SERVICE,
    VROUTE_INSP_RCV_FIND_SERVICE,
    VROUTE_INSP_RCV_FIND_SERVICE_RSP,
    VROUTE_INSP_BUTT
};
typedef void (*vroute_inspect_t)(struct vroute*, void*, vtoken*, uint32_t);

/*
 * for routing table.
 */
struct vroute_ops {
    int  (*join_node)    (struct vroute*, struct sockaddr_in*);
    int  (*find_service) (struct vroute*, vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);
    int  (*probe_service)(struct vroute*, vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);

    int  (*air_service)  (struct vroute*, vsrvcInfo*);
    int  (*reflex)       (struct vroute*, struct sockaddr_in*);
    int  (*probe_connectivity) (struct vroute*, struct sockaddr_in*);
    void (*set_inspect_cb)     (struct vroute*, vroute_inspect_t, void*);
    void (*inspect)      (struct vroute*, vtoken*, uint32_t);
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
    int (*find_service)  (struct vroute*, vnodeConn*, vsrvcHash*);
    int (*find_service_rsp)
                         (struct vroute*, vnodeConn*, vtoken*, vsrvcInfo*);
};

typedef int (*vroute_dht_cb_t)(struct vroute*, vnodeConn*, void*);
struct vroute {
    vnodeId  myid;
    uint32_t props;

    struct vroute_node_space node_space;
    struct vroute_srvc_space srvc_space;
    struct vroute_recr_space recr_space;
    struct vroute_srvc_probe_space srvc_probe_space;

    struct vlock lock;

    struct vroute_ops*     ops;
    struct vroute_dht_ops* dht_ops;
    vroute_dht_cb_t*       cb_ops;
    struct vdht_enc_ops*   enc_ops;
    struct vdht_dec_ops*   dec_ops;

    struct vconfig* cfg;
    struct vmsger*  msger;
    struct vnode*   node;

    /*
     * fields to inspect route details.
     */
    vroute_inspect_t insp_cb;
    void* insp_cookie;
};

int  vroute_init  (struct vroute*, struct vconfig*, struct vhost*, vnodeId*);
void vroute_deinit(struct vroute*);

#endif
