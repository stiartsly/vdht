#ifndef __VRTOUTE_H__
#define __VRTOUTE_H__

#include "vnodeId.h"
#include "varray.h"
#include "vcfg.h"
#include "vsys.h"
#include "vdef.h"

#define NBUCKETS    (VTOKEN_BITLEN)

enum {
    PROP_UNREACHABLE          = 0x00000000,
    PROP_PING                 = 0x00000001,
    PROP_PING_R               = 0x00000002,
    PROP_FIND_NODE            = 0x00000004,
    PROP_FIND_NODE_R          = 0x00000008,
    PROP_FIND_CLOSEST_NODES   = 0x00000010,
    PROP_FIND_CLOSEST_NODES_R = 0x00000020,
    PROP_POST_SERVICE         = 0x00000040,
    PROP_POST_HASH            = 0x00000080,
    PROP_GET_PEERS            = 0x00000100,
    PROP_GET_PEERS_R          = 0x00000200,

    PROP_VER         = 0x00001000,
    PROP_RELAY       = 0x00010000,
    PROP_STUN        = 0x00020000,
    PROP_VPN         = 0x00040000,
    PROP_DDNS        = 0x00080000,
    PROP_MROUTE      = 0x00100000,
    PROP_DHASH       = 0x00200000,
    PROP_APP         = 0x01000000
};

enum {
    PLUGIN_RELAY,
    PLUGIN_STUN,
    PLUGIN_VPN,
    PLUGIN_DDNS,
    PLUGIN_MROUTE,
    PLUGIN_DHASH,
    PLUGIN_APP,
    PLUGIN_BUTT
};

#define PROP_DHT_MASK  ((uint32_t)0x000003ff)

char* vroute_srvc_get_desc(int);

struct vroute;
struct vroute_node_space;
struct vroute_node_space_ops {
    int  (*add_node)     (struct vroute_node_space*, vnodeInfo*);
    int  (*del_node)     (struct vroute_node_space*, vnodeAddr*);
    int  (*get_node)     (struct vroute_node_space*, vnodeId*, vnodeInfo*);
    int  (*get_neighbors)(struct vroute_node_space*, vnodeId*, struct varray*, int);
    int  (*broadcast)    (struct vroute_node_space*, void*);
    int  (*tick)         (struct vroute_node_space*);
    int  (*load)         (struct vroute_node_space*);
    int  (*store)        (struct vroute_node_space*);
    void (*clear)        (struct vroute_node_space*);
    void (*dump)         (struct vroute_node_space*);
};

struct vroute_node_space {
    vnodeInfo* own;
    char db[BUF_SZ];
    int  bucket_sz;
    int  max_snd_tms;
    int  max_rcv_period;

    struct vroute* route;
    struct vroute_node_space_bucket {
        struct varray peers;
        time_t ts;
    } bucket[NBUCKETS];
    struct vroute_node_space_ops* ops;
};

int  vroute_node_space_init  (struct vroute_node_space*, struct vroute*, struct vconfig*, vnodeInfo*);
void vroute_node_space_deinit(struct vroute_node_space*);

/*
 *
 */
struct vroute_srvc_space;
struct vroute_srvc_space_ops {
    int  (*add_srvc_node)(struct vroute_srvc_space*, vsrvcInfo*);
    int  (*get_srvc_node)(struct vroute_srvc_space*, int, vsrvcInfo*);
    void (*clear)        (struct vroute_srvc_space*);
    void (*dump)         (struct vroute_srvc_space*);
};

struct vroute_srvc_space {
    int bucket_sz;

    struct vroute_srvc_space_bucket {
        struct varray srvcs;
        time_t ts;
    } bucket[PLUGIN_BUTT];
    struct vroute_srvc_space_ops* ops;
};

int  vroute_srvc_space_init  (struct vroute_srvc_space*, struct vconfig*);
void vroute_srvc_space_deinit(struct vroute_srvc_space*);

/*
 * for routing table.
 */
struct vroute;
struct vroute_ops {
    int  (*join_node)    (struct vroute*, vnodeAddr*);
    int  (*drop_node)    (struct vroute*, vnodeAddr*);
    int  (*reg_service)  (struct vroute*, int, struct sockaddr_in*);
    int  (*unreg_service)(struct vroute*, int, struct sockaddr_in*);
    int  (*get_service)  (struct vroute*, int, struct sockaddr_in*);
    int  (*kick_nice)    (struct vroute*, int);
    int  (*dsptch)       (struct vroute*, struct vmsg_usr*);
    int  (*load)         (struct vroute*);
    int  (*store)        (struct vroute*);
    int  (*tick)         (struct vroute*);
    void (*clear)        (struct vroute*);
    void (*dump)         (struct vroute*);
};

struct vroute_dht_ops {
    int (*ping)          (struct vroute*, vnodeAddr*);
    int (*ping_rsp)      (struct vroute*, vnodeAddr*, vtoken*, vnodeInfo*);
    int (*find_node)     (struct vroute*, vnodeAddr*, vnodeId*);
    int (*find_node_rsp) (struct vroute*, vnodeAddr*, vtoken*, vnodeInfo*);
    int (*find_closest_nodes)(struct vroute*, vnodeAddr*, vnodeId*);
    int (*find_closest_nodes_rsp)(struct vroute*, vnodeAddr*, vtoken*, struct varray*);
    int (*post_service)  (struct vroute*, vnodeAddr*, vsrvcInfo*);
    int (*post_hash)     (struct vroute*, vnodeAddr*, vnodeHash*);
    int (*get_peers)     (struct vroute*, vnodeAddr*, vnodeHash*);
    int (*get_peers_rsp) (struct vroute*, vnodeAddr*, vtoken*, struct varray*);
};

typedef int (*vroute_dht_cb_t)(struct vroute*, struct sockaddr_in*, void*);
struct vroute {
    vnodeInfo     own_node;
    struct varray own_svcs;
    int nice;

    struct vroute_node_space  node_space;
    struct vroute_srvc_space  srvc_space;

    struct vlock lock;

    struct vroute_ops*     ops;
    struct vroute_dht_ops* dht_ops;
    vroute_dht_cb_t*       cb_ops;

    struct vconfig* cfg;
    struct vmsger*  msger;
};

int  vroute_init  (struct vroute*, struct vconfig*, struct vmsger*, vnodeInfo*);
void vroute_deinit(struct vroute*);

#endif
