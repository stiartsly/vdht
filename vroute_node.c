#include "vglobal.h"
#include "vroute.h"

#define VPEER_TB ((const char*)"dht_peer")

#define vminimum(a, b)   ((a < b) ? a: b)

struct vpeer_flags_desc {
    uint32_t  prop;
    char*     desc;
};

static
struct vpeer_flags_desc peer_flags_desc[] = {
    {PROP_PING,                 "ping"                },
    {PROP_PING_R,               "ping_r"              },
    {PROP_FIND_NODE,            "find_node"           },
    {PROP_FIND_NODE_R,          "find_node_r"         },
    {PROP_FIND_CLOSEST_NODES,   "find_closest_nodes"  },
    {PROP_FIND_CLOSEST_NODES_R, "find_closest_nodes_r"},
    {PROP_POST_SERVICE,         "post_service"        },
    {PROP_POST_HASH,            "post_hash"           },
    {PROP_GET_PEERS,            "get_peers"           },
    {PROP_GET_PEERS_R,          "get_peers_r"         },
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

/*
 * for vpeer
 */
struct vpeer{
    vnodeId   id;
    vnodeVer  ver;
    struct sockaddr_in addr;
    uint32_t  flags;

    time_t    snd_ts;
    time_t    rcv_ts;
    int       ntries;
};

static MEM_AUX_INIT(peer_cache, sizeof(struct vpeer), 0);
static
struct vpeer* vpeer_alloc(void)
{
    struct vpeer* peer = NULL;

    peer = (struct vpeer*)vmem_aux_alloc(&peer_cache);
    vlog((!peer), elog_vmem_aux_alloc);
    retE_p((!peer));
    memset(peer, 0, sizeof(struct vpeer));
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
void vpeer_init(
        struct vpeer* peer,
        vnodeId* id,
        vnodeVer* ver,
        struct sockaddr_in* addr,
        uint32_t flags,
        time_t snd_ts,
        time_t rcv_ts)
{
    vassert(peer);
    vassert(addr);

    vtoken_copy(&peer->id, id );
    vtoken_copy(&peer->ver,ver);
    vsockaddr_copy(&peer->addr, addr);
    peer->flags  = flags;
    peer->snd_ts = snd_ts;
    peer->rcv_ts = rcv_ts;
    peer->ntries = 0;
    return ;
}

static
void vpeer_dump_flags(uint32_t flags)
{
    struct vpeer_flags_desc* desc = peer_flags_desc;
    static char buf[BUF_SZ];
    int found = 0;

    memset(buf, 0, BUF_SZ);
    strcat(buf, "[");
    if (desc->prop & flags) {
        strcat(buf, desc->desc);
        found = 1;
    }
    desc++;
    while(desc->desc) {
        if (desc->prop & flags) {
            strcat(buf, ",");
            strcat(buf, desc->desc);
        }
        desc++;
    }
    if (!found) {
        strcat(buf, "empty");
    }
    strcat(buf, "]");
    vdump(printf("flags:%s", buf));
    return ;
}

static
void vpeer_dump(struct vpeer* peer)
{
    vassert(peer);

    vdump(printf("-> PEER"));
    vtoken_dump(&peer->id);
    vnodeVer_dump(&peer->ver);
    vsockaddr_dump(&peer->addr);
    vpeer_dump_flags(peer->flags);
    vdump(printf("timestamp[snd]: %s",  peer->snd_ts ? ctime(&peer->snd_ts): "not yet"));
    vdump(printf("timestamp[rcv]: %s",  ctime(&peer->rcv_ts)));
    vdump(printf("tried send times:%d", peer->ntries));
    vdump(printf("<- PEER"));
    return ;
}

static
int _aux_space_add_node_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    varg_decl(cookie, 0, vnodeInfo*, node_info);
    varg_decl(cookie, 1, uint32_t*,  min_flags);
    varg_decl(cookie, 2, int*,       max_period);
    varg_decl(cookie, 3, time_t*,    now);
    varg_decl(cookie, 4, int*,       found);
    varg_decl(cookie, 5, struct vpeer**, to);

    if (vtoken_equal(&peer->id, &node_info->id) ||
        vsockaddr_equal(&peer->addr, &node_info->addr)) {
        *to = peer;
        *found = 1;
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

static
int _aux_space_broadcast_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    varg_decl(cookie, 0, struct vroute*, route);
    varg_decl(cookie, 1, void*, svci);
    vnodeAddr addr;

    if (peer->flags == PROP_UNREACHABLE) {
        return 0;
    }
    vnodeAddr_init(&addr, &peer->id, &peer->addr);
    route->dht_ops->post_service(route, &addr, (vsrvcInfo*)svci);
    return 0;
}

static
int _aux_space_tick_cb(void* item, void* cookie)
{
    struct vpeer*  peer  = (struct vpeer*)item;
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, struct vroute*, route);
    varg_decl(cookie, 2, time_t*, now);
    vnodeAddr addr;

    vassert(peer);
    vassert(space);
    vassert(now);

    if (peer->ntries >  space->max_snd_tms) { //unreachable.
        return 0;
    }
    if (peer->ntries == space->max_snd_tms) {
        peer->flags = PROP_UNREACHABLE;
        return 0;
    }
    if ((!peer->snd_ts) ||
        (*now - peer->rcv_ts > space->max_rcv_period)) {
        vnodeAddr_init(&addr, &peer->id, &peer->addr);
        route->dht_ops->ping(route, &addr);
        peer->snd_ts = *now;
        peer->ntries++;
    }
    return 0;
}
/*
 *
 */
static
int _aux_space_load_cb(void* priv, int col, char** value, char** field)
{
    struct vroute_node_space* space = (struct vroute_node_space*)priv;
    char* ip =   (char*)((void**)value)[1];
    char* port = (char*)((void**)value)[2];
    char* id   = (char*)((void**)value)[3];
    char* ver  = (char*)((void**)value)[4];
    vnodeInfo node_info;
    int iport = 0;

    errno = 0;
    iport = strtol(port, NULL, 10);
    retE((errno));

    node_info.flags = 0;
    vtoken_unstrlize(id, &node_info.id);
    vsockaddr_convert(ip, iport, &node_info.addr);
    vnodeVer_unstrlize(ver, &node_info.ver);

    space->ops->add_node(space, &node_info);
    return 0;
}

static
int _aux_space_store_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    sqlite3* db = (sqlite3*)cookie;
    char sql_buf[BUF_SZ];
    char* err = NULL;
    char id[64];
    char ip[64];
    char ver[64];
    int  port = 0;
    int  off = 0;
    int  ret = 0;

    vassert(peer);
    vassert(db);

    memset(id,  0, 64);
    vtoken_strlize(&peer->id, id, 64);
    memset(ip,  0, 64);
    vsockaddr_unconvert(&peer->addr, ip, 64, &port);
    memset(ver, 0, 64);
    vnodeVer_strlize(&peer->ver, ver, 64);

    memset(sql_buf, 0, BUF_SZ);
    off += sprintf(sql_buf + off, "insert into '%s' ", VPEER_TB);
    off += sprintf(sql_buf + off, " ('host', 'port', 'nodeId', 'ver') ");
    off += sprintf(sql_buf + off, " values (");
    off += sprintf(sql_buf + off, " '%s',", ip);
    off += sprintf(sql_buf + off, " '%i',", port);
    off += sprintf(sql_buf + off, " '%s',", id);
    off += sprintf(sql_buf + off, " '%s')", ver);

    ret = sqlite3_exec(db, sql_buf, 0, 0, &err);
    vlog((ret && err), printf("db err:%s\n", err));
    vlog((ret), elog_sqlite3_exec);
    retE((ret));
    return 0;
}

/*
 * the routine to add a node to routing table.
 * @route: routing table.
 * @node:  node address to add to routing table.
 * @flags: properties that node has.
 */
static
int _vroute_node_space_add_node(struct vroute_node_space* space, vnodeInfo* info)
{
    struct varray* peers = NULL;
    struct vpeer*  to    = NULL;
    time_t now = time(NULL);
    uint32_t flags = info->flags;
    uint32_t min_flags = 0;
    int max_period = 0;
    int found = 0;
    int updt  = 0;
    int idx = 0;

    vassert(space);
    vassert(info);

    if (vtoken_equal(&space->own->ver, &info->ver)) {
        flags |= PROP_VER;
    }

    idx = vnodeId_bucket(&space->own->id, &info->id);
    peers = &space->bucket[idx].peers;
    {
        void* argv[] = {
            info,
            &min_flags,
            &max_period,
            &now,
            &found,
            &to
        };
        varray_iterate(peers, _aux_space_add_node_cb, argv);
        if (found) { //found
            vpeer_init(to, &info->id, &info->ver, &info->addr, to->flags | flags, to->snd_ts, now);
            updt = 1;
        } else if (to) { //replace worst one.
            vpeer_init(to, &info->id, &info->ver, &info->addr, flags, 0, now);
            updt = 1;
        } else { // insert new one.
            to = vpeer_alloc();
            vlog((!to), elog_vpeer_alloc);
            retE((!to));
            vpeer_init(to, &info->id, &info->ver, &info->addr, flags, 0, now);
            varray_add_tail(peers, to);
            updt = 1;
        }
        if (updt) {
            space->bucket[idx].ts = now;
        }
    }
    return 0;
}

/*
 * the routine to remove a node given by @node_addr from route table.
 * @route:
 * @node_addr:
 *
 */
static
int _vroute_node_space_del_node(struct vroute_node_space* space, vnodeAddr* addr)
{
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    vnodeAddr peer_addr;
    int idx = 0;
    int i   = 0;

    vassert(space);
    vassert(addr);

    idx = vnodeId_bucket(&space->own->id, &addr->id);
    peers = &space->bucket[idx].peers;
    for (; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        vnodeAddr_init(&peer_addr, &peer->id, &peer->addr);
        if (vnodeAddr_equal(&peer_addr, addr)) {
            break;
        }
    }
    if (i < varray_size(peers)) {
        vpeer_free(varray_del(peers, i));
    }
    return 0;
}

/*
 * the routine to find info structure of give nodeId
 * @route: handle to route table.
 * @targetId: node ID.
 * @info:
 */
static
int _vroute_node_space_get_node(struct vroute_node_space* space, vnodeId* target, vnodeInfo* info)
{
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    int ret = -1;
    int idx = 0;
    int i = 0;

    vassert(space);
    vassert(target);
    vassert(info);

    idx = vnodeId_bucket(&space->own->id, target);
    peers = &space->bucket[idx].peers;
    for (; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vtoken_equal(&peer->id, target)) {
            vnodeInfo_init(info, target, &peer->addr, peer->flags, &peer->ver);
            info->flags &= ~PROP_VER;
            ret = 0;
            break;
        }
    }
    return ret;
}

static
int _vroute_node_space_get_neighbors(struct vroute_node_space* space, vnodeId* target, struct varray* closest, int num)
{
    struct vpeer_track {
        vnodeMetric metric;
        int bucket_idx;
        int item_idx;
    };

    int _aux_space_track_cmp_cb(void* a, void* b, void* cookie)
    {
        struct vpeer_track* item = (struct vpeer_track*)a;
        struct vpeer_track* tgt  = (struct vpeer_track*)b;

        return vnodeMetric_cmp(&tgt->metric, &item->metric);
    }

    void _aux_space_peer_free_cb(void* item, void* cookie)
    {
        struct vpeer_track* track = (struct vpeer_track*)item;
        struct vmem_aux* maux = (struct vmem_aux*)cookie;

        vmem_aux_free(maux, track);
    }

    struct vsorted_array array;
    struct vmem_aux maux;
    int i = 0;
    int j = 0;

    struct vpeer_track* track = NULL;
    struct varray* peers = NULL;
    struct vpeer* item = NULL;

    vassert(space);
    vassert(target);
    vassert(closest);
    vassert(num > 0);

    vmem_aux_init(&maux, sizeof(struct vpeer_track), 8);
    vsorted_array_init(&array, 0,  _aux_space_track_cmp_cb, space);

    for (; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (; j < varray_size(peers); j++) {
            track = (struct vpeer_track*)vmem_aux_alloc(&maux);
            item  = (struct vpeer*)varray_get(peers, j);
            vnodeId_dist(&item->id, target, &track->metric);
            track->bucket_idx = i;
            track->item_idx   = j;
            vsorted_array_add(&array, track);
        }
    }
    for (i = 0; i < vminimum(num, vsorted_array_size(&array)); i++) {
        vnodeInfo* info = NULL;
        uint32_t flags = 0;
        track = (struct vpeer_track*)vsorted_array_get(&array, i);
        peers = &space->bucket[track->bucket_idx].peers;
        item  = (struct vpeer*)varray_get(peers, track->item_idx);
        info  = vnodeInfo_alloc();
        flags = item->flags & ~PROP_VER;
        vnodeInfo_init(info, &item->id, &item->addr, flags, &item->ver);
        varray_add_tail(closest, info);
    }
    vsorted_array_zero(&array, _aux_space_peer_free_cb, &maux);
    vmem_aux_deinit(&maux);
    return 0;
}

static
int _vroute_node_space_broadcast(struct vroute_node_space* space, void* svci)
{
    struct varray* peers = NULL;
    int i = 0;

    vassert(space);
    vassert(svci);

    for (; i < NBUCKETS; i++) {
        void* argv[] = {
            space->route,
            svci
        };
        peers = &space->bucket[i].peers;
        varray_iterate(peers, _aux_space_broadcast_cb, argv);
    }
    return 0;
}

static
int _vroute_node_space_tick(struct vroute_node_space* space)
{
    struct vroute* route = space->route;
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    time_t now = time(NULL);
    vnodeAddr addr;
    int i  = 0;
    vassert(space);

    for (; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            space->route,
            &now
        };

        peers = &space->bucket[i].peers;
        varray_iterate(peers, _aux_space_tick_cb, argv);

        if (varray_size(peers) <= 0) {
            continue;
        }
        if ((space->bucket[i].ts + space->max_rcv_period) >= now) {
            continue;
        }
        peer = (struct vpeer*)varray_get_rand(peers);
        vnodeAddr_init(&addr, &peer->id, &peer->addr);
        route->dht_ops->find_closest_nodes(route, &addr, &space->own->id);
    }
    return 0;
}

/*
 * to load all nodes info from db file to routing table
 * @route:
 * @file : file name for stored db.
 *
 */
static
int _vroute_node_space_load(struct vroute_node_space* space)
{
    char sql_buf[BUF_SZ];
    sqlite3* db = NULL;
    char* err = NULL;
    int ret = 0;
    vassert(space);

    ret = sqlite3_open(space->db, &db);
    vlog((ret), elog_sqlite3_open);
    retE((ret));

    memset(sql_buf, 0, BUF_SZ);
    sprintf(sql_buf, "select * from %s", VPEER_TB);
    ret = sqlite3_exec(db, sql_buf, _aux_space_load_cb, space, &err);
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
int _vroute_node_space_store(struct vroute_node_space* space)
{
    struct varray* peers = NULL;
    sqlite3* db = NULL;
    int ret = 0;
    int i = 0;
    vassert(space);

    ret = sqlite3_open(space->db, &db);
    vlog((ret), elog_sqlite3_open);
    retE((ret));

    for (; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        varray_iterate(peers, _aux_space_store_cb, db);
    }
    sqlite3_close(db);
    vlogI(printf("writeback route infos"));
    return 0;
}

/*
 * to clear all nodes info in routing table
 * @route:
 */
static
void _vroute_node_space_clear(struct vroute_node_space* space)
{
    struct varray* peers = NULL;
    int i  = 0;
    vassert(space);

    for (; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        while(varray_size(peers) > 0) {
            vpeer_free((struct vpeer*)varray_pop_tail(peers));
        }
    }
    return ;
}

/*
 * to dump all dht nodes info in routing table
 * @route:
 */
static
void _vroute_node_space_dump(struct vroute_node_space* space)
{
    struct varray* peers = NULL;
    int i = 0;
    int j = 0;
    vassert(space);

    vdump(printf("-> ROUTING SPACE"));
    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            vpeer_dump((struct vpeer*)varray_get(peers, j));
        }
    }
    vdump(printf("<- ROUTING SPACE"));

    return ;
}

struct vroute_node_space_ops route_space_ops = {
    .add_node      = _vroute_node_space_add_node,
    .del_node      = _vroute_node_space_del_node,
    .get_node      = _vroute_node_space_get_node,
    .get_neighbors = _vroute_node_space_get_neighbors,
    .broadcast     = _vroute_node_space_broadcast,
    .tick          = _vroute_node_space_tick,
    .load          = _vroute_node_space_load,
    .store         = _vroute_node_space_store,
    .clear         = _vroute_node_space_clear,
    .dump          = _vroute_node_space_dump
};

int vroute_node_space_init(struct vroute_node_space* space, struct vroute* route, struct vconfig* cfg, vnodeInfo* node_info)
{
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(cfg);
    vassert(node_info);

    ret += cfg->inst_ops->get_route_db_file(cfg, space->db, BUF_SZ);
    ret += cfg->inst_ops->get_route_bucket_sz(cfg, &space->bucket_sz);
    ret += cfg->inst_ops->get_route_max_snd_tms(cfg, &space->max_snd_tms);
    ret += cfg->inst_ops->get_route_max_rcv_period(cfg, &space->max_rcv_period);
    retE((ret < 0));

    for (; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].peers, 8);
        space->bucket[i].ts = 0;
    }
    space->route = route;
    space->own = node_info;
    space->ops = &route_space_ops;
    return 0;
}

void vroute_node_space_deinit(struct vroute_node_space* space)
{
    int i = 0;
    vassert(space);

    space->ops->clear(space);
    for (; i < NBUCKETS; i++) {
        varray_deinit(&space->bucket[i].peers);
    }
    return ;
}

