#include "vglobal.h"
#include "vroute.h"

#define VPEER_TB ((const char*)"dht_peer")

#define vminimum(a, b)   ((a < b) ? a: b)

/*
 * for vpeer
 */
struct vpeer {
    vnodeInfo node;
    time_t snd_ts;
    time_t rcv_ts;
    int ntries;
};

static MEM_AUX_INIT(peer_cache, sizeof(struct vpeer), 0);
static
struct vpeer* vpeer_alloc(void)
{
    struct vpeer* peer = NULL;

    peer = (struct vpeer*)vmem_aux_alloc(&peer_cache);
    vlog((!peer), elog_vmem_aux_alloc);
    retE_p((!peer));
    memset(peer, 0, sizeof(*peer));
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
void vpeer_init(struct vpeer* peer, vnodeInfo* node, time_t snd_ts, time_t rcv_ts)
{
    vassert(peer);
    vassert(node);

    vnodeInfo_copy(&peer->node, node);
    peer->snd_ts = snd_ts;
    peer->rcv_ts = rcv_ts;
    peer->ntries = 0;
}

static
void vpeer_dump(struct vpeer* peer)
{
    vassert(peer);

    vdump(printf("-> PEER"));
    vnodeInfo_dump(&peer->node);
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
    varg_decl(cookie, 1, int*,       min_weight);
    varg_decl(cookie, 2, int*,       max_period);
    varg_decl(cookie, 3, time_t*,    now);
    varg_decl(cookie, 4, int*,       found);
    varg_decl(cookie, 5, struct vpeer**, to);

    if (vnodeInfo_equal(&peer->node, node_info)) {
        *to = peer;
        *found = 1;
        return 1;
    }

    if ((*now - peer->rcv_ts) > *max_period) {
        *to = peer;
        *max_period = *now - peer->rcv_ts;
    }
    if (peer->node.weight < *min_weight) {
        *to = peer;
        *min_weight = peer->node.weight;
    }
    return 0;
}

static
int _aux_space_broadcast_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, struct vsrvcInfo*, svc);
    struct vroute* route = space->route;

    if (peer->ntries >= space->max_snd_tms) {
        return 0; //unreachable.
    }
    route->dht_ops->post_service(route, &peer->node, svc);
    return 0;
}

static
int _aux_space_tick_cb(void* item, void* cookie)
{
    struct vpeer*  peer  = (struct vpeer*)item;
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, struct vroute*, route);
    varg_decl(cookie, 2, time_t*, now);

    vassert(peer);
    vassert(space);
    vassert(now);

    if (peer->ntries >=  space->max_snd_tms) { //unreachable.
        return 0;
    }
    if ((!peer->snd_ts) ||
        (*now - peer->rcv_ts > space->max_rcv_tmo)) {
        route->dht_ops->ping(route, &peer->node);
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
    char* id    = (char*)((void**)value)[1];
    char* ver   = (char*)((void**)value)[2];
    char* laddr = (char*)((void**)value)[3];
    char* uaddr = (char*)((void**)value)[4];
    char* eaddr = (char*)((void**)value)[5];
    char* raddr = (char*)((void**)value)[6];
    vnodeInfo node_info;

    {
        memset(&node_info, 0, sizeof(node_info));
        vtoken_unstrlize(id, &node_info.id);
        vsockaddr_unstrlize(laddr, &node_info.laddr);
        vsockaddr_unstrlize(uaddr, &node_info.uaddr);
        vsockaddr_unstrlize(eaddr, &node_info.eaddr);
        vsockaddr_unstrlize(raddr, &node_info.raddr);
        vnodeVer_unstrlize(ver, &node_info.ver);
    }

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
    char ver[64];
    char laddr[64];
    char uaddr[64];
    char eaddr[64];
    char raddr[64];
    int  off = 0;
    int  ret = 0;

    vassert(peer);
    vassert(db);

    {
        memset(id,    0, 64);
        memset(ver,   0, 64);
        memset(laddr, 0, 64);
        memset(uaddr, 0, 64);
        memset(eaddr, 0, 64);
        memset(raddr, 0, 64);

        vtoken_strlize   (&peer->node.id,    id,    64);
        vnodeVer_strlize (&peer->node.ver  , ver,   64);
        vsockaddr_strlize(&peer->node.laddr, laddr, 64);
        vsockaddr_strlize(&peer->node.uaddr, uaddr, 64);
        vsockaddr_strlize(&peer->node.eaddr, eaddr, 64);
        vsockaddr_strlize(&peer->node.raddr, raddr, 64);
    }

    memset(sql_buf, 0, BUF_SZ);
    off += sprintf(sql_buf + off, "insert into '%s' ", VPEER_TB);
    off += sprintf(sql_buf + off, " ('nodeId', 'ver', 'laddr', 'uaddr', 'eaddr', 'raddr')");
    off += sprintf(sql_buf + off, " values (");
    off += sprintf(sql_buf + off, " '%s',", id   );
    off += sprintf(sql_buf + off, " '%s',", ver  );
    off += sprintf(sql_buf + off, " '%s',", laddr);
    off += sprintf(sql_buf + off, " '%s',", uaddr);
    off += sprintf(sql_buf + off, " '%s',", eaddr);
    off += sprintf(sql_buf + off, " '%s')", raddr);

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
    int min_weight = 0;
    int max_period = 0;
    int found = 0;
    int updt  = 0;
    int idx = 0;

    vassert(space);
    vassert(info);

    if (vtoken_equal(&space->node_ver, &info->ver)) {
        info->weight++;
    }

    min_weight = info->weight;
    idx = vnodeId_bucket(&space->node_id, &info->id);
    peers = &space->bucket[idx].peers;
    {
        void* argv[] = {
            info,
            &min_weight,
            &max_period,
            &now,
            &found,
            &to
        };
        varray_iterate(peers, _aux_space_add_node_cb, argv);
        if (found) { //found
            vpeer_init(to, info, to->snd_ts, now);
            updt = 1;
        } else if (to && (varray_size(peers) >= space->bucket_sz)) { //replace worst one.
            vpeer_init(to, info, 0, now);
            updt = 1;
        } else { // insert new one.
            to = vpeer_alloc();
            vlog((!to), elog_vpeer_alloc);
            retE((!to));
            vpeer_init(to, info, 0, now);
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

    idx = vnodeId_bucket(&space->node_id, target);
    peers = &space->bucket[idx].peers;
    for (i = 0; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vtoken_equal(&peer->node.id, target)) {
            vnodeInfo_copy(info, &peer->node);
            if (vtoken_equal(&peer->node.ver, &space->node_ver)) {
                //minus because uncareness of version as to other nodes.
                info->weight -= 1;
            }
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
        struct vpeer_track* item = (struct vpeer_track*)b;
        struct vpeer_track* tgt  = (struct vpeer_track*)a;

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

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            track = (struct vpeer_track*)vmem_aux_alloc(&maux);
            item  = (struct vpeer*)varray_get(peers, j);
            vnodeId_dist(&item->node.id, target, &track->metric);
            track->bucket_idx = i;
            track->item_idx   = j;
            vsorted_array_add(&array, track);
        }
    }
    for (i = 0; i < vminimum(num, vsorted_array_size(&array)); i++) {
        vnodeInfo* info = NULL;
        track = (struct vpeer_track*)vsorted_array_get(&array, i);
        peers = &space->bucket[track->bucket_idx].peers;
        item  = (struct vpeer*)varray_get(peers, track->item_idx);
        info  = vnodeInfo_alloc(); //todo;
        vnodeInfo_copy(info, &item->node);
        if (vtoken_equal(&info->ver, &space->node_ver)) {
            info->weight--;
        }
        varray_add_tail(closest, info);
    }
    vsorted_array_zero(&array, _aux_space_peer_free_cb, &maux);
    vmem_aux_deinit(&maux);
    return 0;
}

/*
 *  the routine to broadcast the give service @svci to all nodes in routing
 *  table. which is provied by local node.
 *
 * @space:
 * @svci:  metadata of service.
 */
static
int _vroute_node_space_broadcast(struct vroute_node_space* space, void* svci)
{
    int i = 0;
    vassert(space);
    vassert(svci);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            svci
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_broadcast_cb, argv);
    }
    return 0;
}

/*
 * the routine to activate dynamicness of node routing table. the actions
 * includes send dht @ping msg to all nodes if possible and send @find_closest_nodes
 * msg to a random node if routing table has not been refreshed for a certain
 * time.
 *
 * @space:
 */
static
int _vroute_node_space_tick(struct vroute_node_space* space)
{
    struct vroute* route = space->route;
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    time_t now = time(NULL);
    int i  = 0;
    vassert(space);

    for (i = 0; i < NBUCKETS; i++) {
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
        if ((space->bucket[i].ts + space->max_rcv_tmo) >= now) {
            continue;
        }
        peer = (struct vpeer*)varray_get_rand(peers);
        if (peer->ntries >= space->max_snd_tms) {
            continue;
        }
        route->dht_ops->find_closest_nodes(route, &peer->node, &space->node_id);
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

    vdump(printf("-> NODE ROUTING SPACE"));
    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            vpeer_dump((struct vpeer*)varray_get(peers, j));
        }
    }
    vdump(printf("<- NODE ROUTING SPACE"));

    return ;
}

struct vroute_node_space_ops route_space_ops = {
    .add_node      = _vroute_node_space_add_node,
    .get_node      = _vroute_node_space_get_node,
    .get_neighbors = _vroute_node_space_get_neighbors,
    .broadcast     = _vroute_node_space_broadcast,
    .tick          = _vroute_node_space_tick,
    .load          = _vroute_node_space_load,
    .store         = _vroute_node_space_store,
    .clear         = _vroute_node_space_clear,
    .dump          = _vroute_node_space_dump
};

static
int _aux_space_prepare_db(struct vroute_node_space* space)
{
    char sql_buf[BUF_SZ];
    sqlite3* db = NULL;
    struct stat stat_buf;
    char* err = NULL;
    int ret = 0;

    errno = 0;
    ret = stat(space->db, &stat_buf);
    retS((ret >= 0));

    ret = sqlite3_open(space->db, &db);
    vlog((ret), elog_sqlite3_open);
    retE((ret));

    memset(sql_buf, 0, BUF_SZ);
    ret += sprintf(sql_buf + ret, "CREATE TABLE '%s' (", VPEER_TB);
    ret += sprintf(sql_buf + ret, "'id' INTEGER PRIOMARY_KEY,");
    ret += sprintf(sql_buf + ret, "'nodeId' TEXT,");
    ret += sprintf(sql_buf + ret, "'ver'   TEXT,");
    ret += sprintf(sql_buf + ret, "'laddr' TEXT,");
    ret += sprintf(sql_buf + ret, "'uaddr' TEXT,");
    ret += sprintf(sql_buf + ret, "'eaddr' TEXT,");
    ret += sprintf(sql_buf + ret, "'raddr' TEXT)");

    ret = sqlite3_exec(db, sql_buf, NULL, NULL, &err);
    vlog((ret && err), printf("db err:%s\n", err));
    vlog((ret), elog_sqlite3_exec);
    sqlite3_close(db);
    return 0;
}

int vroute_node_space_init(struct vroute_node_space* space, struct vroute* route, struct vconfig* cfg, vnodeId* my_id)
{
    vnodeVer my_ver;
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(cfg);
    vassert(my_id);

    ret = cfg->ext_ops->get_route_db_file(cfg, space->db, BUF_SZ);
    retE((ret < 0));
    space->bucket_sz   = cfg->ext_ops->get_route_bucket_sz(cfg);
    space->max_snd_tms = cfg->ext_ops->get_route_max_snd_tms(cfg);
    space->max_rcv_tmo = cfg->ext_ops->get_route_max_rcv_tmo(cfg);

    ret = _aux_space_prepare_db(space);
    retE((ret < 0));

    for (; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].peers, 8);
        space->bucket[i].ts = 0;
    }

    vnodeVer_unstrlize(vhost_get_version(), &my_ver);
    vtoken_copy(&space->node_ver, &my_ver);
    vtoken_copy(&space->node_id,  my_id);
    space->route = route;
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

