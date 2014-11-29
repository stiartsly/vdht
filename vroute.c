#include "vglobal.h"
#include "vroute.h"

#define VPEER_TB ((const char*)"dht_peer")

#define MAX_CAPC ((int)8)
#define vmin(a, b)   ((a < b) ? a: b)

struct vpeer_flags_desc {
    uint32_t  prop;
    char*     desc;
};

static char* dht_id_desc[] = {
    "ping",
    "ping rsp",
    "find_node",
    "find_node rsp",
    "find_closest_nodes",
    "find_closest_nodes rsp",
    "post_service",
    "post_hash",
    "get_peers",
    "get_peers rsp",
    "get_plugin",
    "get_plugin_rsp",
    NULL,
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

static uint32_t peer_service_prop[] = {
    PROP_RELAY,
    PROP_STUN,
    PROP_VPN,
    PROP_DDNS,
    PROP_MROUTE,
    PROP_DHASH,
    PROP_APP
};

struct vplugin_desc {
    int   what;
    char* desc;
};

static
struct vplugin_desc plugin_desc[] = {
    { PLUGIN_RELAY,  "relay"           },
    { PLUGIN_STUN,   "stun"            },
    { PLUGIN_VPN,    "vpn"             },
    { PLUGIN_DDNS,   "ddns"            },
    { PLUGIN_MROUTE, "multiple routes" },
    { PLUGIN_DHASH,  "data hash"       },
    { PLUGIN_APP,    "app"             },
    { PLUGIN_BUTT, 0}
};

char* vpluger_get_desc(int what)
{
    struct vplugin_desc* desc = plugin_desc;

    for (; desc->desc; ) {
        if (desc->what == what) {
            break;
        }
        desc++;
    }
    if (!desc->desc) {
        return "unkown plugin";
    }
    return desc->desc;
}

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
        struct sockaddr_in* addr,
        uint32_t flags,
        time_t snd_ts,
        time_t rcv_ts)
{
    vassert(peer);
    vassert(addr);

    vtoken_copy(&peer->id, id);
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
    varg_decl(cookie, 0, struct vroute_space*, space);
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
        (*now - peer->rcv_ts > MAX_RCV_PERIOD)) {
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
    struct vroute_space* space = (struct vroute_space*)priv;
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
int _vroute_space_add_node(struct vroute_space* space, vnodeInfo* info)
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
            vpeer_init(to, &info->id, &info->addr, to->snd_ts, now, to->flags | flags);
            updt = 1;
        } else if (to) { //replace worst one.
            vpeer_init(to, &info->id, &info->addr, 0, now, flags);
            updt = 1;
        } else { // insert new one.
            to = vpeer_alloc();
            vlog((!to), elog_vpeer_alloc);
            retE((!to));
            vpeer_init(to, &info->id, &info->addr, 0, now, flags);
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
int _vroute_space_del_node(struct vroute_space* space, vnodeAddr* addr)
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
int _vroute_space_get_node(struct vroute_space* space, vnodeId* target, vnodeInfo* info)
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
int _vroute_space_get_neighbors(struct vroute_space* space, vnodeId* target, struct varray* closest, int num)
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

    vmem_aux_init(&maux, sizeof(struct vpeer_track), 0);
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
    for (i = 0; i < vmin(num, vsorted_array_size(&array)); i++) {
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
int _vroute_space_broadcast(struct vroute_space* space, void* svci)
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
int _vroute_space_tick(struct vroute_space* space)
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
        if ((space->bucket[i].ts + MAX_RCV_PERIOD) >= now) {
            continue;
        }
        peer = varray_get_rand(peers);
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
int _vroute_space_load(struct vroute_space* space)
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
int _vroute_space_store(struct vroute_space* space)
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
void _vroute_space_clear(struct vroute_space* space)
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
void _vroute_space_dump(struct vroute_space* space)
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

struct vroute_space_ops route_space_ops = {
    .add_node      = _vroute_space_add_node,
    .del_node      = _vroute_space_del_node,
    .get_node      = _vroute_space_get_node,
    .get_neighbors = _vroute_space_get_neighbors,
    .broadcast     = _vroute_space_broadcast,
    .tick          = _vroute_space_tick,
    .load          = _vroute_space_load,
    .store         = _vroute_space_store,
    .clear         = _vroute_space_clear,
    .dump          = _vroute_space_dump
};

int vroute_space_init(struct vroute_space* space, struct vroute* route, struct vconfig* cfg, vnodeInfo* node_info)
{
    int i = 0;

    vassert(space);
    vassert(cfg);
    vassert(node_info);

    cfg->ops->get_str_ext(cfg, "route.db_file", space->db, BUF_SZ, DEF_ROUTE_DB_FILE);
    cfg->ops->get_int_ext(cfg, "route.bucket_size", &space->bucket_sz, DEF_ROUTE_BUCKET_CAPC);
    cfg->ops->get_int_ext(cfg, "route.max_send_times", &space->max_snd_tms, DEF_ROUTE_MAX_SND_TIMES);

    for (; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].peers, 8);
        space->bucket[i].ts = 0;
    }
    space->route = route;
    space->own = node_info;
    space->ops = &route_space_ops;
    return 0;
}

void vroute_space_deinit(struct vroute_space* space)
{
    int i = 0;
    vassert(space);

    space->ops->clear(space);
    for (; i < NBUCKETS; i++) {
        varray_deinit(&space->bucket[i].peers);
    }
    return ;
}


/*
 * for plug_item
 */
struct vservice {
    struct sockaddr_in addr;
    int what;
    uint32_t flags;
    time_t rcv_ts;
};

static MEM_AUX_INIT(service_cache, sizeof(struct vservice), 8);
static
struct vservice* vservice_alloc(void)
{
    struct vservice* item = NULL;

    item = (struct vservice*)vmem_aux_alloc(&service_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    return item;
}

static
void vservice_free(struct vservice* item)
{
    vassert(item);
    vmem_aux_free(&service_cache, item);
    return ;
}

static
void vservice_init(struct vservice* item, int what, struct sockaddr_in* addr, time_t ts, uint32_t flags)
{
    vassert(item);
    vassert(addr);

    vsockaddr_copy(&item->addr, addr);
    item->what   = what;
    item->rcv_ts = ts;
    item->flags  = flags;
    return ;
}

static
void vservice_dump(struct vservice* item)
{
    vassert(item);
    vdump(printf("-> SERVICE"));
    vdump(printf("category:%s", vpluger_get_desc(item->what)));
    vdump(printf("timestamp[rcv]: %s",  ctime(&item->rcv_ts)));
    vsockaddr_dump(&item->addr);
    vdump(printf("<- SERVICE"));
    return;
}

static
int _aux_mart_add_service_cb(void* item, void* cookie)
{
    struct vservice* svc = (struct vservice*)item;
    varg_decl(cookie, 0, vsrvcInfo*, svci);
    varg_decl(cookie, 1, struct vservice**, to);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int32_t*, min_tdff);
    varg_decl(cookie, 4, int*, found);

    if ((svc->what == svci->usage)
        && vsockaddr_equal(&svc->addr, &svci->addr)) {
        *to = svc;
        *found = 1;
        return 1;
    }
    if ((*now - svc->rcv_ts) < *min_tdff) {
        *to = svc;
        *min_tdff = *now - svc->rcv_ts;
    }
    return 0;
}

static
int _aux_mart_get_service_cb(void* item, void* cookie)
{
    struct vservice* svc = (struct vservice*)item;
    varg_decl(cookie, 1, struct vservice**, to);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int32_t*, min_tdff);

    if ((*now - svc->rcv_ts) < *min_tdff) {
        *to = svc;
        *min_tdff = *now - svc->rcv_ts;
    }
    return 0;
}

static
int _vroute_mart_add_service_node(struct vroute_mart* mart, vsrvcInfo* svci)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int32_t min_tdff = (int32_t)0x7fffffff;
    int found = 0;
    int updt = 0;
    int ret  = -1;

    vassert(mart);
    vassert(svci);
    retE((svci->usage < 0));
    retE((svci->usage >= PLUGIN_BUTT));

    svcs = &mart->bucket[svci->usage].srvcs;
    {
        void* argv[] = {
            svci,
            &to,
            &now,
            &min_tdff,
            &found
        };
        varray_iterate(svcs, _aux_mart_add_service_cb, argv);
        if (found) {
            to->rcv_ts = now;
            updt = 1;
        } else if (to) {
            vservice_init(to, svci->usage, &svci->addr, now, 0);
            updt = 1;
        } else {
            to = vservice_alloc();
            retE((!to));
            vservice_init(to, svci->usage, &svci->addr, now, 0);
            varray_add_tail(svcs, to);
            updt = 1;
        };
    }
    if (updt) {
        mart->bucket[svci->usage].ts = now;
        ret = 0;
    }
    return ret;
}

static
int _vroute_mart_get_serivce_node(struct vroute_mart* mart, int what, vsrvcInfo* svci)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int32_t min_tdff = (int32_t)0x7fffffff;
    int ret = -1;

    vassert(mart);
    vassert(svci);

    svcs = &mart->bucket[what].srvcs;
    {
        void* argv[] = {
            &to,
            &now,
            &min_tdff
        };
        varray_iterate(svcs, _aux_mart_get_service_cb, argv);
        if (to) {
            vsrvcInfo_init(svci, what, &to->addr);
            ret = 0;
        }
    }
    return ret;
}

/*
 * clear all plugs
 * @pluger: handler to plugin structure.
 */
static
void _vroute_mart_clear(struct vroute_mart* mart)
{
    struct varray* svcs = NULL;
    int i = 0;
    vassert(mart);

    for (; i < PLUGIN_BUTT; i++) {
        svcs = &mart->bucket[i].srvcs;
        while(varray_size(svcs) > 0) {
            vservice_free((struct vservice*)varray_del(svcs, 0));
        }
    }
    return;
}

/*
 * dump pluger
 * @pluger: handler to plugin structure.
 */
static
void _vroute_mart_dump(struct vroute_mart* mart)
{
    struct varray* svcs = NULL;
    int i = 0;
    int j = 0;

    vassert(mart);

    vdump(printf("-> ROUTING MART"));
    for (i = 0; i < PLUGIN_BUTT; i++) {
        svcs = &mart->bucket[i].srvcs;
        for (j = 0; j < varray_size(svcs); j++) {
            vservice_dump((struct vservice*)varray_get(svcs, j));
        }
    }
    vdump(printf("<- ROUTING MART"));
    return;
}

static
struct vroute_mart_ops route_mart_ops = {
    .add_srvc_node = _vroute_mart_add_service_node,
    .get_srvc_node = _vroute_mart_get_serivce_node,
    .clear         = _vroute_mart_clear,
    .dump          = _vroute_mart_dump
};

int vroute_mart_init(struct vroute_mart* mart, struct vconfig* cfg)
{
    int i = 0;

    vassert(mart);
    vassert(cfg);

    for (; i < PLUGIN_BUTT; i++) {
        varray_init(&mart->bucket[i].srvcs, 8);
        mart->bucket[i].ts = 0;
    }
    cfg->ops->get_int_ext(cfg, "route.bucket_size", &mart->bucket_sz, DEF_ROUTE_BUCKET_CAPC);
    mart->ops = &route_mart_ops;

    return 0;
}

void vroute_mart_deinit(struct vroute_mart* mart)
{
    int i = 0;

    mart->ops->clear(mart);
    for (; i < PLUGIN_BUTT; i++) {
        varray_deinit(&mart->bucket[i].srvcs);
    }
    return ;
}

int _aux_route_tick_cb(void* item, void* cookie)
{
    struct vroute_space* space = (struct vroute_space*)cookie;
    int ret = 0;
    vassert(space);

    ret = space->ops->broadcast(space, item);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to add a node to routing table.
 * @route: routing table.
 * @node:  node address to add to routing table.
 * @flags: properties that node has.
 */
static
int _vroute_join_node(struct vroute* route, vnodeAddr* node_addr)
{
    struct vroute_space* space = &route->space;
    vnodeInfo node_info;
    vnodeVer ver;
    int ret = 0;

    vassert(route);
    vassert(node_addr);

    memset(&ver, 0, sizeof(ver));
    vnodeInfo_init(&node_info, &node_addr->id, &node_addr->addr, 0, &ver);

    vlock_enter(&route->lock);
    ret = space->ops->add_node(space, &node_info);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to remove a node given by @node_addr from route table.
 * @route:
 * @node_addr:
 *
 */
static
int _vroute_drop_node(struct vroute* route, vnodeAddr* node_addr)
{
    struct vroute_space* space = &route->space;
    int ret = 0;

    vassert(route);
    vassert(node_addr);

    vlock_enter(&route->lock);
    ret = space->ops->del_node(space, node_addr);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

static
int _vroute_reg_service(struct vroute* route, int what, struct sockaddr_in* addr)
{
    vsrvcInfo* svc = NULL;
    int i = 0;

    vassert(route);
    vassert(addr);
    retE((what < 0));
    retE((what >= PLUGIN_BUTT));

    vlock_enter(&route->lock);
    route->own_node.flags |= peer_service_prop[what];
    for (; i < varray_size(&route->own_svcs); i++){
        svc = (vsrvcInfo*)varray_get(&route->own_svcs, i);
        if ((svc->usage == what) &&
            (vsockaddr_equal(&svc->addr, addr))) {
            break;
        }
    }
    if (i >= varray_size(&route->own_svcs)) {
        svc = vsrvcInfo_alloc();
        vlog((!svc), elog_vsrvcInfo_alloc);
        retE((!svc));
        vsrvcInfo_init(svc, what, addr);
        varray_add_tail(&route->own_svcs, svc);
    }
    vlock_leave(&route->lock);
    return 0;
}

static
int _vroute_unreg_service(struct vroute* route, int what, struct sockaddr_in* addr)
{
    vsrvcInfo* svc = NULL;
    int i = 0;

    vassert(route);
    vassert(addr);
    retE((what < 0));
    retE((what >= PLUGIN_BUTT));

    vlock_enter(&route->lock);
    route->own_node.flags &= ~(peer_service_prop[what]);
    for (; i < varray_size(&route->own_svcs); i++) {
        svc = (vsrvcInfo*)varray_get(&route->own_svcs, i);
        if ((svc->usage == what) &&
            (vsockaddr_equal(&svc->addr, addr))) {
            break;
        }
    }
    if (i < varray_size(&route->own_svcs)) {
        svc = (vsrvcInfo*)varray_del(&route->own_svcs, i);
        vsrvcInfo_free(svc);
    }
    vlock_leave(&route->lock);
    return 0;
}

static
int _vroute_get_service(struct vroute* route, int what, struct sockaddr_in* addr)
{
    struct vroute_mart* mart = &route->mart;
    vsrvcInfo svc;
    int ret = 0;

    vassert(route);
    vassert(addr);
    retE((what < 0));
    retE((what >= PLUGIN_BUTT));

    vlock_enter(&route->lock);
    ret = mart->ops->get_srvc_node(mart, what, &svc);
    vlock_leave(&route->lock);
    retE((ret < 0));

    vsockaddr_copy(addr, &svc.addr);
    return 0;
}

static
int _vroute_load(struct vroute* route)
{
    struct vroute_space* space = &route->space;
    int ret = 0;
    vassert(route);

    vlock_enter(&route->lock);
    ret = space->ops->load(space);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

static
int _vroute_store(struct vroute* route)
{
    struct vroute_space* space = &route->space;
    int ret = 0;
    vassert(route);

    vlock_enter(&route->lock);
    ret = space->ops->store(space);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

static
int _vroute_tick(struct vroute* route)
{
    struct vroute_space* space = &route->space;
    vassert(route);

    vlock_enter(&route->lock);
    space->ops->tick(space);
    varray_iterate(&route->own_svcs, _aux_route_tick_cb, space);
    vlock_leave(&route->lock);
    return 0;
}

static
void _vroute_clear(struct vroute* route)
{
    struct vroute_space* space = &route->space;
    struct vroute_mart*  mart  = &route->mart;
    vassert(route);

    vlock_enter(&route->lock);
    space->ops->clear(space);
    mart->ops->clear(mart);
    vlock_leave(&route->lock);
    return;
}

static
void _vroute_dump(struct vroute* route)
{
    struct vroute_space* space = &route->space;
    struct vroute_mart*  mart  = &route->mart;
    int i = 0;
    vassert(route);

    vdump(printf("-> ROUTE"));
    vlock_enter(&route->lock);
    vdump(printf("-> MINE"));
    vdump(printf("-> NODE"));
    vnodeInfo_dump(&route->own_node);
    vdump(printf("<- NODE"));
    vdump(printf("-> SERVICES"));
    for (; i < varray_size(&route->own_svcs); i++) {
        vsrvcInfo_dump((vsrvcInfo*)varray_get(&route->own_svcs, i));
    }
    vdump(printf("<- SERVICES"));
    vdump(printf("<- MINE"));

    space->ops->dump(space);
    mart->ops->dump(mart);
    vlock_leave(&route->lock);
    vdump(printf("<- ROUTE"));
    return;
}

static
int _vroute_dispatch(struct vroute* route, struct vmsg_usr* mu)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    void* ctxt = NULL;
    int what = 0;
    int ret = 0;
    vassert(route);
    vassert(mu);

    what = dec_ops->dec(mu->data, mu->len, &ctxt);
    retE((what >= VDHT_UNKNOWN));
    retE((what < 0));
    vlogI(printf("Received (@%s) msg", dht_id_desc[what]));

    ret = route->cb_ops[what](route, &mu->addr->vsin_addr, ctxt);
    dec_ops->dec_done(ctxt);
    retE((ret < 0));
    return 0;
}

static
struct vroute_ops route_ops = {
    .join_node     = _vroute_join_node,
    .drop_node     = _vroute_drop_node,
    .reg_service   = _vroute_reg_service,
    .unreg_service = _vroute_unreg_service,
    .get_service   = _vroute_get_service,
    .dsptch        = _vroute_dispatch,
    .load          = _vroute_load,
    .store         = _vroute_store,
    .tick          = _vroute_tick,
    .clear         = _vroute_clear,
    .dump          = _vroute_dump
};

static
int _vroute_dht_ping(struct vroute* route, vnodeAddr* dest)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->ping(&token, &route->own_node.id, buf, vdht_buf_len());
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
    vlogI(printf("send @ping"));
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
    }
    vlogI(printf("send @ping_rsp"));
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

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->find_node(&token, &route->own_node.id, target, buf, vdht_buf_len());
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
    vlogI(printf("send @find_node"));
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

    ret = enc_ops->find_node_rsp(token, &route->own_node.id, info, buf, vdht_buf_len());
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
    vlogI(printf("send @find_node_rsp"));
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

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->find_closest_nodes(&token, &route->own_node.id, target, buf, vdht_buf_len());
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
    vlogI(printf("send @find_closest_nodes"));
    return 0;
}

/*
 * @route:
 * @dest:
 * @token:
 * @closest:
 */
static
int _vroute_dht_find_closest_nodes_rsp(
        struct vroute* route,
        vnodeAddr* dest,
        vtoken* token,
        struct varray* closest)
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

    ret = enc_ops->find_closest_nodes_rsp(token, &route->own_node.id, closest, buf, vdht_buf_len());
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
    vlogI(printf("send @find_closest_nodes_rsp"));
    return 0;
}

static
int _vroute_dht_post_service(struct vroute* route, vnodeAddr* dest, vsrvcInfo* service)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(service);

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->post_service(&token, &route->own_node.id, service, buf, vdht_buf_len());
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
    vlogI(printf("send @post_service"));
    return 0;
}

/*
 * @route:
 * @dest :
 * @hash :
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

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->get_peers(&token, &route->own_node.id, hash, buf, vdht_buf_len());
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
    vlogI(printf("send @get_peers"));
    return 0;
}

/*
 * @route:
 * @dest:
 * @token:
 * @peers:
 */
static
int _vroute_dht_get_peers_rsp(
        struct vroute* route,
        vnodeAddr* dest,
        vtoken* token,
        struct varray* peers)
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

    ret = enc_ops->get_peers_rsp(token, &route->own_node.id, peers, buf, vdht_buf_len());
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
    vlogI(printf("send @get_peers_rsp"));
    return 0;
}

static
struct vroute_dht_ops route_dht_ops = {
    .ping                   = _vroute_dht_ping,
    .ping_rsp               = _vroute_dht_ping_rsp,
    .find_node              = _vroute_dht_find_node,
    .find_node_rsp          = _vroute_dht_find_node_rsp,
    .find_closest_nodes     = _vroute_dht_find_closest_nodes,
    .find_closest_nodes_rsp = _vroute_dht_find_closest_nodes_rsp,
    .post_service           = _vroute_dht_post_service,
    .post_hash              = _vroute_dht_post_hash,
    .get_peers              = _vroute_dht_get_peers,
    .get_peers_rsp          = _vroute_dht_get_peers_rsp
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
    struct vroute_space* space = &route->space;
    vnodeAddr addr;
    vnodeInfo info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&addr.addr, from);
    ret = dec_ops->ping(ctxt, &token, &addr.id);
    retE((ret < 0));
    vnodeInfo_init(&info, &addr.id, &addr.addr, PROP_PING, NULL);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));
    ret = route->dht_ops->ping_rsp(route, &addr, &token, &route->own_node);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_ping_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_space* space = &route->space;
    vnodeInfo info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    ret = dec_ops->ping_rsp(ctxt, &token, &info);
    retE((ret < 0));
    info.flags |= PROP_PING_R;
    vsockaddr_copy(&info.addr, from);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_node(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_space* space = &route->space;
    struct varray closest;
    vnodeAddr addr;
    vnodeInfo info;
    vnodeId target;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&addr.addr, from);
    ret = dec_ops->find_node(ctxt, &token, &addr.id, &target);
    retE((ret < 0));
    vnodeInfo_init(&info, &addr.id, &addr.addr, PROP_FIND_NODE, NULL);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));

    ret = space->ops->get_node(space, &target, &info);
    if (ret >= 0) {
        ret = route->dht_ops->find_node_rsp(route, &addr, &token, &info);
        retE((ret < 0));
        return 0;
    }
    // otherwise, send @find_closest_nodes_rsp response instead.
    varray_init(&closest, MAX_CAPC);
    ret = space->ops->get_neighbors(space, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, &addr, &token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_node_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_space* space = &route->space;
    vnodeInfo info;
    vnodeAddr addr;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&addr.addr, from);
    ret = dec_ops->find_node_rsp(ctxt, &token, &addr.id, &info);
    retE((ret < 0));
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));

    vnodeInfo_init(&info, &addr.id, &addr.addr, PROP_FIND_NODE_R, NULL);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_closest_nodes(
        struct vroute* route,
        struct sockaddr_in* from,
        void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_space* space = &route->space;
    struct varray closest;
    vnodeAddr addr;
    vnodeInfo info;
    vnodeId target;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&addr.addr, from);
    ret = dec_ops->find_closest_nodes(ctxt, &token, &addr.id, &target);
    retE((ret < 0));
    vnodeInfo_init(&info, &addr.id, &addr.addr, PROP_FIND_CLOSEST_NODES, NULL);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));

    varray_init(&closest, MAX_CAPC);
    ret = space->ops->get_neighbors(space, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, &addr, &token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_closest_nodes_rsp(
        struct vroute* route,
        struct sockaddr_in* from,
        void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_space* space = &route->space;
    struct varray closest;
    vnodeInfo info;
    vnodeAddr addr;
    vtoken token;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&addr.addr, from);
    varray_init(&closest, MAX_CAPC);
    ret = dec_ops->find_closest_nodes_rsp(ctxt, &token, &addr.id, &closest);
    retE((ret < 0));

    for (; i < varray_size(&closest); i++) {
        space->ops->add_node(space, (vnodeInfo*)varray_get(&closest, i));
    }
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);

    vnodeInfo_init(&info, &addr.id, &addr.addr, PROP_FIND_CLOSEST_NODES_R, NULL);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_post_service(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_space* space = &route->space;
    struct vroute_mart*  mart  = &route->mart;
    vsrvcInfo svc;
    vnodeAddr addr;
    vnodeInfo info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&addr.addr, from);
    ret = dec_ops->post_service(ctxt, &token, &addr.id, &svc);
    retE((ret < 0));
    retE((svc.usage < 0));
    retE((svc.usage >= PLUGIN_BUTT));

    ret = mart->ops->add_srvc_node(mart, &svc);
    retE((ret < 0));
    vnodeInfo_init(&info, &addr.id, &addr.addr, peer_service_prop[svc.usage], NULL);
    ret = space->ops->add_node(space, &info);
    retE((ret < 0));
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
int _vroute_cb_get_peers(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(ctxt);

    //todo;
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
vroute_dht_cb_t route_cb_ops[] = {
    _vroute_cb_ping,
    _vroute_cb_ping_rsp,
    _vroute_cb_find_node,
    _vroute_cb_find_node_rsp,
    _vroute_cb_find_closest_nodes,
    _vroute_cb_find_closest_nodes_rsp,
    _vroute_cb_post_service,
    _vroute_cb_post_hash,
    _vroute_cb_get_peers,
    _vroute_cb_get_peers_rsp,
    NULL
};

static
int _aux_route_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vroute* route = (struct vroute*)cookie;
    int ret = 0;
    vassert(route);
    vassert(mu);

    ret = route->ops->dsptch(route, mu);
    retE((ret < 0));
    return 0;
}

int vroute_init(struct vroute* route, struct vconfig* cfg, struct vmsger* msger, vnodeInfo* own_info)
{
    vassert(route);
    vassert(msger);
    vassert(own_info);

    vnodeInfo_copy(&route->own_node, own_info);
    route->own_node.flags = PROP_DHT_MASK;
    varray_init(&route->own_svcs, 4);

    vlock_init(&route->lock);
    vroute_space_init(&route->space, route, cfg, &route->own_node);
    vroute_mart_init (&route->mart, cfg);

    route->ops     = &route_ops;
    route->dht_ops = &route_dht_ops;
    route->cb_ops  = route_cb_ops;

    route->cfg   = cfg;
    route->msger = msger;

    msger->ops->add_cb(route->msger, route, _aux_route_msg_cb, VMSG_DHT);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    int i = 0;
    vassert(route);

    vroute_space_deinit(&route->space);
    vroute_mart_deinit(&route->mart);
    for (; i < varray_size(&route->own_svcs); i++) {
        vsrvcInfo_free((vsrvcInfo*)varray_get(&route->own_svcs, i));
    }
    varray_deinit(&route->own_svcs);
    vlock_deinit(&route->lock);
    return ;
}

