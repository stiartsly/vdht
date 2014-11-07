#ifndef __VRTOUTE_H__
#define __VRTOUTE_H__

#include "vnodeId.h"
#include "varray.h"
#include "vsys.h"

#define MAX_SND_PERIOD 600
#define MAX_RCV_PERIOD 1500
#define MAX_SND_TIMES  10

#define BUCKET_CAPC ((int)10)
#define NBUCKETS    ((int)160)

enum {
    PROP_UNREACHABLE = 0x00000000,
    PROP_PING        = 0x00000001,
    PROP_PING_R      = 0x00000002,
    PROP_FIND_NODE   = 0x00000004,
    PROP_FIND_NODE_R = 0x00000008,
    PROP_GET_PEERS   = 0x00000010,
    PROP_GET_PEERS_R = 0x00000020,
    PROP_POST_HASH   = 0x00000040,
    PROP_POST_HASH_R = 0x00000080,
    PROP_FIND_CLOSEST_NODES   = 0x00000100,
    PROP_FIND_CLOSEST_NODES_R = 0x00000200,

    PROP_VER         = 0x00001000,
    PROP_RELAY       = 0x00010000,
    PROP_STUN        = 0x00020000,
    PROP_VPN         = 0x00040000,
    PROP_DDNS        = 0x00080000,
    PROP_MROUTE      = 0x00100000,
    PROP_DHASH       = 0x00200000,
    PROP_APP         = 0x01000000
};

#define PROP_PLUG_MASK ((uint32_t)0x003f0000)
enum {
    PLUG_RELAY,
    PLUG_STUN,
    PLUG_VPN,
    PLUG_DDNS,
    PLUG_MROUTE,
    PLUG_DHASH,
    PLUG_APP,
    PLUG_BUTT
};

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

struct vroute_plugin_ops {
    int (*plug)  (struct vroute*, int);
    int (*unplug)(struct vroute*, int);
    int (*get)   (struct vroute*, int, vnodeAddr*);
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
    struct vroute_cb_ops* cb_ops;
    struct vroute_dht_ops*  dht_ops;
    struct vroute_plugin_ops* plugin_ops;

    struct vbucket bucket[NBUCKETS];
    struct vlock   lock;

    struct vmsger*  msger;
    struct vmem_aux mbuf_cache;
};

int  vroute_init  (struct vroute*, struct vmsger*, vnodeAddr*);
void vroute_deinit(struct vroute*);

#endif
