#include "vglobal.h"
#include "vroute.h"

#define VPEER_TB ((const char*)"dht_peer")

#define MAX_CAPC ((int)8)
#define vmin(a, b)   ((a < b) ? a: b)

struct vpeer{
    vnodeAddr extId;
    vnodeVer  ver;
    uint32_t  flags;

    time_t    snd_ts;
    time_t    rcv_ts;
    int       ntries;
};

struct vpeer_flags_desc {
    uint32_t  prop;
    char*     desc;
};

static char* dht_id_desc[] = {
    "ping",
    "ping rsp",
    "find_node",
    "find_node rsp",
    "get_peers",
    "get_peers rsp",
    "post_hash",
    "post_hash_r",
    "find_closest_nodes",
    "find_closest_nodes rsp",
    NULL,
};

static
struct vpeer_flags_desc peer_flags_desc[] = {
    {PROP_PING,                 "ping"                },
    {PROP_PING_R,               "ping_r"              },
    {PROP_FIND_NODE,            "find_node"           },
    {PROP_FIND_NODE_R,          "find_node_r"         },
    {PROP_GET_PEERS,            "get_peers"           },
    {PROP_GET_PEERS_R,          "get_peers_r"         },
    {PROP_POST_HASH,            "post_hash"           },
    {PROP_POST_HASH_R,          "post_hash_r"         },
    {PROP_FIND_CLOSEST_NODES,   "find_closest_nodes"  },
    {PROP_FIND_CLOSEST_NODES_R, "find_closest_nodes_r"},
    {PROP_VER,                  "same version"        },
    {PROP_RELAY,                "relay"               },
    {PROP_STUN,                 "stun"                },
    {PROP_VPN,                  "vpn"                 },
    {PROP_DDNS,                 "ddns"                },
    {PROP_MROUTE,               "mroute"              },
    {PROP_DHASH,                "dhash"               },
    {PROP_APP,                  "apps"                },
    {PROP_UNREACHABLE, 0}
};

static uint32_t peer_plugin_prop[] = {
    PROP_RELAY,
    PROP_STUN,
    PROP_VPN,
    PROP_DDNS,
    PROP_MROUTE,
    PROP_DHASH,
    PROP_APP,
    PROP_PLUG_MASK
};

/*
 * for vpeer
 */
static MEM_AUX_INIT(peer_cache, sizeof(struct vpeer), 0);
static
struct vpeer* vpeer_alloc(void)
{
    struct vpeer* peer = NULL;

    peer = (struct vpeer*)vmem_aux_alloc(&peer_cache);
    vlog((!peer), elog_vmem_aux_alloc);
    retE_p((!peer));
    return peer;
}
/*
 * @peer:
 */
static
void vpeer_free(struct vpeer* peer)
{
    vassert(peer);
    vmem_aux_free(&peer_cache, peer);
    return ;
}

/*
 * @peer:
 * @addr:
 * @snd_ts:
 * @rcv_ts:
 * @flags:
 */
static
void vpeer_init(struct vpeer* peer, vnodeAddr* addr, time_t snd_ts, time_t rcv_ts, int flags)
{
    vassert(peer);
    vassert(addr);

    vnodeAddr_copy(&peer->extId, addr);
    peer->snd_ts = snd_ts;
    peer->rcv_ts = rcv_ts;
    peer->flags  = flags;
    peer->ntries = 0;
    return ;
}

static
void vpeer_dump_flags(uint32_t flags)
{
    struct vpeer_flags_desc* desc = peer_flags_desc;
    static char buf[BUF_SZ];

    memset(buf, 0, BUF_SZ);
    while(desc->desc) {
        if (desc->prop & flags) {
            strcat(buf, desc->desc);
            strcat(buf, ":");
        }
        desc++;
    }
    vdump(printf("flags: %s", buf));
    return ;
}

static
void vpeer_dump(struct vpeer* peer)
{
    char buf[64];
    int  port = 0;
    vassert(peer);

    vdump(printf("-> PEER"));
    vnodeId_dump(&peer->extId.id);
    vsockaddr_unconvert(&peer->extId.addr, buf, 64, &port);
    vdump(printf("addr:%s:%d", buf,port));
    vpeer_dump_flags(peer->flags);
    vdump(printf("timestamp[snd]: %s",  peer->snd_ts ? ctime(&peer->snd_ts): "not yet"));
    vdump(printf("timestamp[rcv]: %s",  ctime(&peer->rcv_ts)));
    vdump(printf("tried send times:%d", peer->ntries));
    vdump(printf("<- PEER"));
    return ;
}

static
int _aux_route_add_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    vnodeAddr* addr      = (vnodeAddr*)((void**)cookie)[0];
    uint32_t* min_flags  =  (uint32_t*)((void**)cookie)[1];
    int*      max_period = (int*)((void**)cookie)[2];
    time_t*   now        = (time_t*)((void**)cookie)[3];
    int*      found      = (int*)((void**)cookie)[4];
    struct vpeer** to    = (struct vpeer**)((void**)cookie)[5];

    vassert(peer);
    vassert(addr);
    vassert(min_flags);
    vassert(max_period);
    vassert(now);
    vassert(found);
    vassert(to);

    if (vnodeId_equal(&peer->extId.id, &addr->id)) {
        *found = 1;
        *to = peer;
        return 1;
    }

    if ((*now - peer->rcv_ts) > *max_period) {
        *to = peer;
        *max_period = *now - peer->rcv_ts;
    }
    if (peer->flags < *min_flags) {
        *to = peer;
        *min_flags = peer->flags;
    }
    return 0;
}

/*
 *
 */
static
int _aux_route_load_cb(void* priv, int col, char** value, char** field)
{
    struct vroute* route = (struct vroute*)priv;
    char* ip =   (char*)((void**)value)[1];
    char* port = (char*)((void**)value)[2];
    char* id   = (char*)((void**)value)[3];
    vnodeAddr node_addr;
    int iport = 0;

    vassert(route);
    vassert(ip && port && id);

    errno = 0;
    iport = strtol(port, NULL, 10);
    retE((errno));
    vsockaddr_convert(ip, iport, &node_addr.addr);
    vnodeId_unstrlize(id, &node_addr.id);

    route->ops->add(route, &node_addr, 0);
    return 0;
}

static
int _aux_route_store_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    sqlite3* db = (sqlite3*)cookie;
    char sql_buf[BUF_SZ];
    char* err = NULL;
    char id[64];
    char ip[64];
    int  port = 0;
    int  off = 0;
    int  ret = 0;

    vassert(peer);
    vassert(db);

    memset(id, 0, 64);
    vnodeId_strlize(&peer->extId.id, id, 64);
    memset(ip, 0, 64);
    vsockaddr_unconvert(&peer->extId.addr, ip, 64, &port);

    off += sprintf(sql_buf + off, "insert into '%s' ", VPEER_TB);
    off += sprintf(sql_buf + off, " ('host', 'port', 'nodeId') ");
    off += sprintf(sql_buf + off, " values (");
    off += sprintf(sql_buf + off, " '%s',", ip);
    off += sprintf(sql_buf + off, " '%i',", port);
    off += sprintf(sql_buf + off, " '%s')", id);

    ret = sqlite3_exec(db, sql_buf, 0, 0, &err);
    vlog((ret && err), printf("db err:%s\n", err));
    vlog((ret), elog_sqlite3_exec);
    retE((ret));
    return 0;
}

static
int _aux_route_tick_cb(void* item, void* cookie)
{
    struct vpeer*  peer  = (struct vpeer*)item;
    struct vroute* route = (struct vroute*)((void**)cookie)[0];
    time_t* now  = (time_t*)((void**)cookie)[1];

    vassert(peer);
    vassert(route);
    vassert(now);

    if (peer->ntries > route->max_snd_times) { //unreacheable.
        vassert(peer->snd_ts);
        vassert(PROP_UNREACHABLE == peer->flags);
    } else if (peer->ntries == route->max_snd_times) {
        vassert(peer->snd_ts);
        peer->flags = PROP_UNREACHABLE;
    } else {
        if ((!peer->snd_ts) || (*now - peer->rcv_ts > MAX_RCV_PERIOD)) {
            route->dht_ops->ping(route, &peer->extId);
            peer->snd_ts = *now;
            peer->ntries++;
        }
    }
    return 0;
}

static
int _aux_route_dump_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    vassert(peer);

    vpeer_dump(peer);
    return 0;
}

/*
 * the routine to add a node to routing table.
 * @route: routing table.
 * @node:  node address to add to routing table.
 * @flags: properties that node has.
 */
static
int _vroute_add(struct vroute* route, vnodeAddr* node, int flags)
{
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    struct vpeer*  to    = NULL;
    time_t now = time(NULL);
    uint32_t min_flags = 0;
    int max_period = 0;
    int found = 0;
    int updt  = 0;
    int idx = 0;

    vassert(route);
    vassert(node);

    idx = vnodeId_bucket(&route->ownId.id, &node->id);
    vlock_enter(&route->lock);
    peers = &route->bucket[idx].peers;
    {
        void* argv[] = {
            node,
            &min_flags,
            &max_period,
            &now,
            &found,
            &to
        };
        varray_iterate(peers, _aux_route_add_cb, argv);
        if (found) {
            vpeer_init(to, node, to->snd_ts, now, to->flags | flags);
            updt = 1;
        } else if (to) {
            vpeer_init(to, node, 0, now, flags);
            updt = 1;
        } else {
            peer = vpeer_alloc();
            vlog((!peer), elog_vpeer_alloc);
            ret1E((!peer), vlock_leave(&route->lock));

            vpeer_init(peer, node, 0, now, flags);
            varray_add_tail(peers, peer);
            updt = 1;
        }
        if (updt) {
            route->bucket[idx].ts = now;
        }
    }
    vlock_leave(&route->lock);
    return 0;
}

/*
 * the routine to remove a node given by @node_addr from route table.
 * @route:
 * @node_addr:
 *
 */
static
int _vroute_remove(struct vroute* route, vnodeAddr* node_addr)
{
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    int idx = 0;
    int i   = 0;

    vassert(route);
    vassert(node_addr);

    idx = vnodeId_bucket(&route->ownId.id, &node_addr->id);
    vlock_enter(&route->lock);
    peers = &route->bucket[idx].peers;
    for (; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vnodeAddr_equal(&peer->extId, node_addr)) {
            break;
        }
    }
    if (i < varray_size(peers)) {
        varray_del(peers, i);
        vpeer_free(peer);
    }
    vlock_leave(&route->lock);
    return 0;
}

/*
 * the routine to find info structure of give nodeId
 * @route: handle to route table.
 * @targetId: node ID.
 * @info:
 */
static
int _vroute_find(struct vroute* route, vnodeId* targetId, vnodeInfo* info)
{
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    int idx = 0;
    int i = 0;

    vassert(route);
    vassert(targetId);
    vassert(info);

    idx = vnodeId_bucket(&route->ownId.id, targetId);
    vlock_enter(&route->lock);
    peers = &route->bucket[idx].peers;
    for (; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vnodeId_equal(&peer->extId.id, targetId)) {
            vnodeInfo_init(info, targetId, &peer->extId.addr, peer->flags, &peer->ver);
            info->flags &= ~PROP_VER;
            break;
        }
        peer = NULL;
    }
    vlock_leave(&route->lock);
    return (peer ? 0 : -1);
}

/*
 * to load all nodes info from db file to routing table
 * @route:
 * @file : file name for stored db.
 *
 */
static
int _vroute_load(struct vroute* route)
{
    char sql_buf[BUF_SZ];
    sqlite3* db = NULL;
    char* err = NULL;
    int ret = 0;
    vassert(route);

    ret = sqlite3_open(route->db, &db);
    vlog((ret), elog_sqlite3_open);
    retE((ret));

    sprintf(sql_buf, "select * from %s", VPEER_TB);
    ret = sqlite3_exec(db, sql_buf, _aux_route_load_cb, route, &err);
    vlog((ret && err), printf("db err:%s\n", err));
    vlog((ret), elog_sqlite3_exec);
    sqlite3_close(db);
    retE((ret));
    return 0;
}

/*
 * to store all nodes info in routing table back to db file.
 * @route:
 * @file
 */
static
int _vroute_store(struct vroute* route)
{
    struct varray* peers = NULL;
    sqlite3* db = NULL;
    int ret = 0;
    int i = 0;
    vassert(route);

    ret = sqlite3_open(route->db, &db);
    vlog((ret), elog_sqlite3_open);
    retE((ret));

    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        peers = &route->bucket[i].peers;
        varray_iterate(peers, _aux_route_store_cb, db);
    }
    vlock_leave(&route->lock);
    sqlite3_close(db);
    vlogI(printf("writeback route infos"));
    return 0;
}

/*
 * to clear all nodes info in routing table
 * @route:
 */
static
int _vroute_clear(struct vroute* route)
{
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    int i  = 0;
    vassert(route);

    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        peers = &route->bucket[i].peers;
        while(varray_size(peers) > 0) {
            peer = (struct vpeer*)varray_pop_tail(peers);
            vpeer_free(peer);
        }
    }
    vlock_leave(&route->lock);
    return 0;
}

/*
 * to dump all dht nodes info in routing table
 * @route:
 */
static
int _vroute_dump(struct vroute* route)
{
    struct varray* peers = NULL;
    int i = 0;
    vassert(route);

    vdump(printf("-> ROUTE"));
    vpeer_dump_flags(route->flags);
    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        peers = &route->bucket[i].peers;
        varray_iterate(peers, _aux_route_dump_cb, route);
    }
    vlock_leave(&route->lock);
    vdump(printf("<- ROUTE"));

    return 0;
}

/*
 *
 */
static
int _vroute_tick(struct vroute* route)
{
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    time_t now = time(NULL);
    int i  = 0;
    vassert(route);

    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        void* argv[] = {
            route,
            &now
        };
        peers = &route->bucket[i].peers;
        varray_iterate(peers, _aux_route_tick_cb, argv);

        if ((varray_size(peers) > 0)
          && (route->bucket[i].ts + MAX_RCV_PERIOD < now)) {
            peer = (struct vpeer*)varray_get_rand(peers);
            route->dht_ops->find_closest_nodes(route, &peer->extId, &route->ownId.id);
        }
    }
    vlock_leave(&route->lock);
    return 0;
}

static
int _vroute_get_peers(struct vroute* route, vnodeHash* hash, struct varray* array, int num)
{
    vassert(route);
    vassert(hash);
    vassert(array);
    vassert(num > 0);

    //todo;
    return 0;
}

static
int _vroute_find_closest_nodes(struct vroute* route, vnodeId* targetId, struct varray* closest, int num)
{
    struct vpeer_track {
        vnodeMetric metric;
        int bucket_idx;
        int item_idx;
    };

    int _aux_track_cmp_cb(void* a, void* b, void* cookie)
    {
        type_decl(struct vpeer_track*, item, a);
        type_decl(struct vpeer_track*, tgt,  b);
        vassert(item);
        vassert(tgt);

        return vnodeMetric_cmp(&tgt->metric, &item->metric);
    }

    void _aux_vpeer_free_cb(void* item, void* cookie)
    {
        type_decl(struct vpeer_track*, track, item  );
        type_decl(struct vmem_aux*   , maux,  cookie);
        vassert(maux);
        vassert(maux);

        vmem_aux_free(maux, track);
        return ;
    }

    struct vsorted_array array;
    struct vmem_aux maux;
    int i = 0;
    int j = 0;

    struct vpeer_track* track = NULL;
    struct varray* peers = NULL;
    struct vpeer* item = NULL;

    vassert(route);
    vassert(targetId);
    vassert(closest);
    vassert(num > 0);

    vmem_aux_init(&maux, sizeof(struct vpeer_track), 0);
    vsorted_array_init(&array, 0,  _aux_track_cmp_cb, route);

    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        peers = &route->bucket[i].peers;
        for (; j < varray_size(peers); j++) {
            track = (struct vpeer_track*)vmem_aux_alloc(&maux);
            item  = (struct vpeer*)varray_get(peers, j);
            vnodeId_dist(&item->extId.id, targetId, &track->metric);
            track->bucket_idx = i;
            track->item_idx   = j;
            vsorted_array_add(&array, track);
        }
    }
    for (i = 0; i < vmin(num, vsorted_array_size(&array)); i++) {
        vnodeInfo* info = NULL;
        uint32_t flags = 0;
        track = (struct vpeer_track*)vsorted_array_get(&array, i);
        peers = &route->bucket[track->bucket_idx].peers;
        item  = (struct vpeer*)varray_get(peers, track->item_idx);
        info  = vnodeInfo_alloc();
        flags = item->flags & ~PROP_VER;
        vnodeInfo_init(info, &item->extId.id, &item->extId.addr, flags, &item->ver);
        varray_add_tail(closest, info);
    }
    vlock_leave(&route->lock);
    vsorted_array_zero(&array, _aux_vpeer_free_cb, &maux);
    vmem_aux_deinit(&maux);
    return 0;
}

static
struct vroute_ops route_ops = {
    .add    = _vroute_add,
    .remove = _vroute_remove,
    .find   = _vroute_find,
    .load   = _vroute_load,
    .store  = _vroute_store,
    .clear  = _vroute_clear,
    .dump   = _vroute_dump,
    .tick   = _vroute_tick,

    .get_peers          = _vroute_get_peers,
    .find_closest_nodes = _vroute_find_closest_nodes
};

/*
 *
 */
static
int _vroute_plug(struct vroute* route, int plgnId)
{
    vassert(route);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    route->flags |= peer_plugin_prop[plgnId];
    return 0;
}

static
int _vroute_unplug(struct vroute* route, int plgnId)
{
    uint32_t flags = peer_plugin_prop[plgnId];
    vassert(route);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    route->flags &= ~flags;
    return 0;
}

static
int _vroute_get_plugin(struct vroute* route, int plgnId, vnodeAddr* addr)
{
    uint32_t flags = peer_plugin_prop[plgnId];
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    int found = 0;
    int i = 0;
    int j = 0;

    vassert(route);
    vassert(addr);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        peers = &route->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            peer = (struct vpeer*)varray_get(peers, j);
            if (peer->flags & flags) {
                found = 1;
                break;
            }
        }
        if (found) {
            break;
        }
    }
    if (found) {
        vnodeAddr_copy(addr, &peer->extId);
    }
    vlock_leave(&route->lock);
    return (found ? 0 : -1);
}

struct vroute_plugin_ops route_plugin_ops = {
    .plug   = _vroute_plug,
    .unplug = _vroute_unplug,
    .get    = _vroute_get_plugin
};

/*
 * the routine to pack and send ping query to target node (asynchronizedly)
 * @route:
 * @dest : address to target node.
 */
static
int _vroute_dht_ping(struct vroute* route, vnodeAddr* dest)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);

    vtoken_make(&token);
    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->ping(&token, &route->ownId.id, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
        vlogI(printf("send 'ping'"));
    }
    return 0;
}

/*
 * the routine to pack and send ping response back to source node.
 * @route:
 * @from :
 * @info :
 */
static
int _vroute_dht_ping_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, vnodeInfo* info)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(info);

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->ping_rsp(token, info, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
        vlogI(printf("send 'ping rsp'"));
    }
    return 0;
}

/*
 * the routine to pack and send @find_node query to destination node.
 * @route
 * @dest:
 * @target
 */
static
int _vroute_dht_find_node(struct vroute* route, vnodeAddr* dest, vnodeId* target)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(target);

    vtoken_make(&token);
    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_node(&token, &route->ownId.id, target, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
        vlogI(printf("send 'find_node'"));
    }
    return 0;
}

/*
 * @route
 * @
 */
static
int _vroute_dht_find_node_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, vnodeInfo* info)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(info);

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_node_rsp(token, &route->ownId.id, info, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
        vlogI(printf("send 'find_node rsp'"));
    }
    return 0;
}

/*
 * @route:
 * @dest :
 * @hash :
 */
static
int _vroute_dht_get_peers(struct vroute* route, vnodeAddr* dest, vnodeHash* hash)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(hash);

    vtoken_make(&token);
    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->get_peers(&token, &route->ownId.id, hash, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
        vlogI(printf("send 'get_peers'"));
    }
    return 0;
}

/*
 * @route:
 * @dest:
 * @token:
 * @peers:
 */
static
int _vroute_dht_get_peers_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, struct varray* peers)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(peers);

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->get_peers_rsp(token, &route->ownId.id, peers, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
        vlogI(printf("send 'get_peers' rsp"));
    }
    return 0;
}

/*
 * @route:
 * @dest :
 * @hash
 */
static
int _vroute_dht_post_hash(struct vroute* route, vnodeAddr* dest, vnodeHash* hash)
{
    vassert(route);
    vassert(dest);
    vassert(hash);

    //todo;
    return 0;
}

/*
 * @route:
 * @dest:
 * @hash
 */
static
int _vroute_dht_post_hash_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, struct varray* hashes)
{
    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(hashes);

    //todo;
    return 0;
}

/*
 * @route:
 * @dest:
 * @targetId:
 */
static
int _vroute_dht_find_closest_nodes(struct vroute* route, vnodeAddr* dest, vnodeId* target)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(target);

    vtoken_make(&token);
    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_closest_nodes(&token, &route->ownId.id, target, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    return 0;
}

/*
 * @route:
 * @dest:
 * @token:
 * @closest:
 */
static
int _vroute_dht_find_closest_nodes_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, struct varray* closest)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void*  buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(closest);

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_closest_nodes_rsp(token, &route->ownId.id, closest, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&dest->addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    return 0;
}

static
struct vroute_dht_ops route_dht_ops = {
    .ping                   = _vroute_dht_ping,
    .ping_rsp               = _vroute_dht_ping_rsp,
    .find_node              = _vroute_dht_find_node,
    .find_node_rsp          = _vroute_dht_find_node_rsp,
    .get_peers              = _vroute_dht_get_peers,
    .get_peers_rsp          = _vroute_dht_get_peers_rsp,
    .post_hash              = _vroute_dht_post_hash,
    .post_hash_rsp          = _vroute_dht_post_hash_rsp,
    .find_closest_nodes     = _vroute_dht_find_closest_nodes,
    .find_closest_nodes_rsp = _vroute_dht_find_closest_nodes_rsp
};


static
void _aux_vnodeInfo_free(void* info, void* cookie)
{
    vassert(info);
    vnodeInfo_free((vnodeInfo*)info);
    return ;
}

/*
 * @route:
 * @closest:
 */

static
int _vroute_cb_ping(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    vnodeAddr* me = &route->ownId;
    vnodeAddr node;
    vnodeInfo info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&node.addr, from);
    ret = dec_ops->ping(ctxt, &token, &node.id);
    retE((ret < 0));
    ret = route->ops->add(route, &node, PROP_PING);
    retE((ret < 0));
    vnodeInfo_init(&info, &me->id, &me->addr, route->flags, &route->version);
    ret = route->dht_ops->ping_rsp(route, &node, &token, &info);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_ping_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    uint32_t flags = 0;
    vnodeInfo info;
    vnodeAddr node;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    ret = dec_ops->ping_rsp(ctxt, &token, &info);
    retE((ret < 0));

    flags = (info.flags | PROP_PING_R);
    vnodeAddr_init(&node, &info.id, &info.addr);
    if (vnodeVer_equal(&info.ver, &route->version)) {
        flags |=  PROP_VER;
    }
    ret = route->ops->add(route, &node, flags);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_node(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray closest;
    vnodeAddr node;
    vnodeInfo info;
    vnodeId target;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&node.addr, from);
    ret = dec_ops->find_node(ctxt, &token, &node.id, &target);
    retE((ret < 0));
    ret = route->ops->add(route, &node, PROP_FIND_NODE);
    retE((ret < 0));
    ret = route->ops->find(route, &target, &info);
    if (ret >= 0) {
        ret = route->dht_ops->find_node_rsp(route, &node, &token, &info);
        retE((ret < 0));
        return 0;
    }
    // otherwise, send @find_closest_nodes_rsp response instead.
    varray_init(&closest, MAX_CAPC);
    ret = route->ops->find_closest_nodes(route, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, &node, &token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_node_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    vnodeInfo info;
    vnodeAddr node;
    vtoken token;
    uint32_t flags = 0;
    int ret = 0;

    vsockaddr_copy(&node.addr, from);
    ret = dec_ops->find_node_rsp(ctxt, &token, &node.id, &info);
    retE((ret < 0));
    ret = route->ops->add(route, &node, PROP_FIND_NODE_R);
    retE((ret < 0));

    flags = info.flags;
    vnodeAddr_init(&node, &info.id, &info.addr);
    if (vnodeVer_equal(&info.ver, &route->version)) {
        flags |= PROP_VER;
    }
    ret = route->ops->add(route, &node, flags);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_get_peers(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray peers;
    vnodeHash hash;
    vnodeAddr node;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&node.addr, from);
    ret = dec_ops->get_peers(ctxt, &token, &node.id, &hash);
    retE((ret < 0));
    ret = route->ops->add(route, &node, PROP_GET_PEERS);
    retE((ret < 0));

    varray_init(&peers, MAX_CAPC);
    ret = route->ops->get_peers(route, &hash, &peers, MAX_CAPC);
    if (ret >= 0) {
        ret = route->dht_ops->get_peers_rsp(route, &node, &token, &peers);
        varray_zero(&peers, _aux_vnodeInfo_free, NULL);
        varray_deinit(&peers);
        retE((ret < 0));
        return 0;
    }
    // otherwise, send @find_closest_nodes_rsp instead.
    ret = route->ops->find_closest_nodes(route, &node.id, &peers, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&peers));
    ret = route->dht_ops->find_closest_nodes_rsp(route, &node, &token, &peers);
    varray_zero(&peers, _aux_vnodeInfo_free, NULL);
    varray_deinit(&peers);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_get_peers_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(ctxt);

    //TODO;
    return 0;
}

static
int _vroute_cb_post_hash(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(ctxt);

    //TODO;
    return 0;
}

static
int _vroute_cb_post_hash_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(ctxt);

    //TODO;
    return 0;
}

static
int _vroute_cb_find_closest_nodes(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray closest;
    vnodeAddr node;
    vnodeId target;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&node.addr, from);
    ret = dec_ops->find_closest_nodes(ctxt, &token, &node.id, &target);
    retE((ret < 0));
    ret = route->ops->add(route, &node, PROP_FIND_CLOSEST_NODES);
    retE((ret < 0));

    varray_init(&closest, MAX_CAPC);
    ret = route->ops->find_closest_nodes(route, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));

    ret = route->dht_ops->find_closest_nodes_rsp(route, &node, &token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_closest_nodes_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray closest;
    vnodeAddr node;
    vtoken token;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&node.addr, from);
    varray_init(&closest, MAX_CAPC);
    ret = dec_ops->find_closest_nodes_rsp(ctxt, &token, &node.id, &closest);
    retE((ret < 0));
    ret = route->ops->add(route, &node, PROP_FIND_CLOSEST_NODES_R);
    if (ret < 0) {
        varray_zero(&closest, _aux_vnodeInfo_free, NULL);
        varray_deinit(&closest);
        return -1;
    }

    for (; i < varray_size(&closest); i++) {
        vnodeInfo* info = NULL;
        uint32_t  flags = 0;

        info = (vnodeInfo*)varray_get(&closest, i);
        vnodeAddr_init(&node, &info->id, &info->addr);
        flags = info->flags;
        if (vnodeVer_equal(&info->ver, &route->version)) {
            flags = info->flags | PROP_VER;
        }
        route->ops->add(route, &node, flags);
    }
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    return 0;
}

static
struct vroute_cb_ops route_cb_ops = {
    .ping                   = _vroute_cb_ping,
    .ping_rsp               = _vroute_cb_ping_rsp,
    .find_node              = _vroute_cb_find_node,
    .find_node_rsp          = _vroute_cb_find_node_rsp,
    .get_peers              = _vroute_cb_get_peers,
    .get_peers_rsp          = _vroute_cb_get_peers_rsp,
    .post_hash              = _vroute_cb_post_hash,
    .post_hash_rsp          = _vroute_cb_post_hash_rsp,
    .find_closest_nodes     = _vroute_cb_find_closest_nodes,
    .find_closest_nodes_rsp = _vroute_cb_find_closest_nodes_rsp
};

static
int _aux_route_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vroute* route = (struct vroute*)cookie;
    void* ctxt = NULL;
    int ret = 0;

    vroute_dht_cb_t routine[] = {
        route->cb_ops->ping,
        route->cb_ops->ping_rsp,
        route->cb_ops->find_node,
        route->cb_ops->find_node_rsp,
        route->cb_ops->get_peers,
        route->cb_ops->get_peers_rsp,
        route->cb_ops->post_hash,
        route->cb_ops->post_hash_rsp,
        route->cb_ops->find_closest_nodes,
        route->cb_ops->find_closest_nodes_rsp,
    };

    vassert(route);
    vassert(mu);

    ret = dht_dec_ops.dec(mu->data, mu->len, &ctxt);
    retE((ret >= VDHT_UNKNOWN));
    retE((ret < 0));
    vlogI(printf("Received (%s) msg.\n", dht_id_desc[ret]));

    ret = routine[ret](route, &mu->addr->vsin_addr, ctxt);
    dht_dec_ops.dec_done(ctxt);
    retE((ret < 0));
    return 0;
}

int vroute_init(struct vroute* route, struct vconfig* cfg, struct vmsger* msger, vnodeAddr* addr, vnodeVer* ver)
{
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(msger);
    vassert(addr);

    vnodeAddr_copy(&route->ownId, addr);
    vnodeVer_copy (&route->version, ver);
    route->flags   |= PROP_DHT_MASK;
    route->cfg      = cfg;
    route->msger    = msger;
    route->ops      = &route_ops;
    route->dht_ops  = &route_dht_ops;
    route->cb_ops   = &route_cb_ops;
    route->plugin_ops = &route_plugin_ops;

    ret += cfg->ops->get_str_ext(cfg, "route.db_file", route->db, BUF_SZ, DEF_ROUTE_DB_FILE);
    ret += cfg->ops->get_int_ext(cfg, "route.bucket_size", &route->bucket_sz, DEF_ROUTE_BUCKET_CAPC);
    ret += cfg->ops->get_int_ext(cfg, "route.max_send_times", &route->max_snd_times, DEF_ROUTE_MAX_SND_TIMES);
    retE((ret < 0));

    vlock_init(&route->lock);
    for (; i < NBUCKETS; i++) {
        varray_init(&route->bucket[i].peers, 0);
        route->bucket[i].ts = 0;
    }
    msger->ops->add_cb(route->msger, route, _aux_route_msg_cb, VMSG_DHT);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    int i = 0;
    vassert(route);

    route->ops->clear(route);
    for (; i < NBUCKETS; i++) {
        varray_deinit(&route->bucket[i].peers);
    }
    vlock_deinit(&route->lock);
    return ;
}

