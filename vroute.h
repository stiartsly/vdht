#ifndef __VRTOUTE_H__
#define __VRTOUTE_H__

#include "vnodeId.h"
#include "varray.h"
#include "vsys.h"

#define MAX_SND_PERIOD 600
#define MAX_RCV_PERIOD 1500
#define MAX_SND_TIMES  10

#define BUCKET_CAPC ((int)160)
#define NBUCKETS    ((int)10)

enum {
    VDHT_UNREACHABLE     = 0x00000000,
    VDHT_PING            = 0x00000001,
    VDHT_PING_REPLY      = 0x00000002,
    VDHT_FIND_NODE       = 0x00000004,
    VDHT_FIND_NODE_REPLY = 0x00000008,
    VDHT_GET_PEERS       = 0x00000010,
    VDHT_GET_PEERS_REPLY = 0x00000020,
    VDHT_FIND_CLOSEST_NODES       = 0x00000040,
    VDHT_FIND_CLOSEST_NODES_REPLY = 0x00000080,

    VDHT_VERSION         = 0x00000100,
    VDHT_STUN            = 0x00001000,
    VDHT_RELAY           = 0x00002000,
    VDHT_VPN             = 0x00004000,
    VDHT_APP             = 0x00010000
};

struct vpeer{
    vnodeAddr extId;
    vnodeVer  ver;
    uint32_t  flags;

    time_t    snd_ts;
    time_t    rcv_ts;
    int       ntries;
};

struct vpeer* vpeer_alloc(void);
void vpeer_free(struct vpeer* );
void vpeer_init(struct vpeer*, vnodeAddr*, time_t, time_t, int);

/*
 * for routing table.
 */
struct vroute;
struct vroute_ops {
    int (*load)  (struct vroute*, const char*);
    int (*store) (struct vroute*, const char*);
    int (*clear) (struct vroute*);
    int (*dump)  (struct vroute*);
    int (*tick)  (struct vroute*);

    int (*add)   (struct vroute*, vnodeAddr*, int);
    int (*remove)(struct vroute*, vnodeAddr*);
    int (*find)  (struct vroute*, vnodeId*, vnodeInfo*);

    int (*get_peers)(struct vroute*, vnodeHash*, struct varray*, int);
    int (*find_closest_nodes)(struct vroute*, vnodeId*, struct varray*, int);
};

struct vroute_dht_ops {
    int (*ping)              (struct vroute*, vnodeAddr*);
    int (*ping_rsp)          (struct vroute*, vnodeAddr*, vtoken*, vnodeInfo*);
    int (*find_node)         (struct vroute*, vnodeAddr*, vnodeId*);
    int (*find_node_rsp)     (struct vroute*, vnodeAddr*, vtoken*, vnodeInfo*);
    int (*get_peers)         (struct vroute*, vnodeAddr*, vnodeHash*);
    int (*get_peers_rsp)     (struct vroute*, vnodeAddr*, vtoken*, struct varray*);
    int (*find_closest_nodes)(struct vroute*, vnodeAddr*, vnodeId*);
    int (*find_closest_nodes_rsp)(struct vroute*, vnodeAddr*, vtoken*, struct varray*);
};

struct vroute_cb_ops {
    int (*ping)              (struct vroute*, vtoken*, vnodeAddr*);
    int (*ping_rsp)          (struct vroute*, vnodeAddr*, vnodeInfo*);
    int (*find_node)         (struct vroute*, vtoken*, vnodeAddr*, vnodeId*);
    int (*find_node_rsp)     (struct vroute*, vnodeAddr*, vnodeInfo*);
    int (*get_peers)         (struct vroute*, vtoken*, vnodeAddr*, vnodeHash*);
    int (*get_peers_rsp)     (struct vroute*, vnodeAddr*, struct varray*);
    int (*find_closest_nodes)(struct vroute*, vtoken*,  vnodeAddr*, vnodeId*);
    int (*find_closest_nodes_rsp)(struct vroute*, vnodeAddr*, struct varray*);

};

struct vbucket{
    struct varray peers;
    time_t ts;
};

struct vroute {
    vnodeAddr ownId;
    vnodeVer  version;
    uint32_t  flags;

    struct vroute_ops* ops;
    struct vroute_cb_ops*  cb_ops;
    struct vroute_dht_ops* dht_ops;
    struct vdht_enc_ops*   dht_enc_ops;
    struct vdht_dec_ops*   dht_dec_ops;

    struct vbucket bucket[NBUCKETS];
    struct vlock   lock;

    struct vmsger*  msger;
    struct vmem_aux msg_buf_caches;
#if 0
    struct varray queries;
    struct vlock  query_lock;

#endif
};

int  vroute_init  (struct vroute*, struct vmsger*, vnodeAddr*);
void vroute_deinit(struct vroute*);

#endif
