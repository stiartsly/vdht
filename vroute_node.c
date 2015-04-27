#include "vglobal.h"
#include "vroute.h"

#define VPEER_TB ((const char*)"dht_peer")
/*
 * for vpeer
 */
static MEM_AUX_INIT(peer_cache, sizeof(struct vpeer), 0);
static
struct vpeer* vpeer_alloc(void)
{
    struct vpeer* peer = NULL;
    vnodeInfo*   nodei = NULL;

    nodei = (vnodeInfo*)vnodeInfo_alloc();
    retE_p((!nodei));

    peer = (struct vpeer*)vmem_aux_alloc(&peer_cache);
    vlogEv((!peer), elog_vmem_aux_alloc);
    ret1E_p((!peer), vnodeInfo_free(nodei));
    memset(peer, 0, sizeof(*peer));
    peer->nodei = nodei;
    return peer;
}
/*
 * @peer:
 */
static
void vpeer_free(struct vpeer* peer)
{
    vassert(peer);

    if (peer->nodei) {
        vnodeInfo_free(peer->nodei);
    }
    vmem_aux_free(&peer_cache, peer);
    return ;
}

static
int vpeer_init(struct vpeer* peer, struct sockaddr_in* local, vnodeInfo* nodei, time_t rcv_ts, int direct)
{
    vassert(peer);
    vassert(nodei);

    vnodeConn_set(&peer->conn, local, &nodei->addrs[nodei->naddrs-1]);
    vnodeInfo_copy(peer->nodei, nodei);
    peer->rcv_ts = direct ? rcv_ts : 0;
    peer->ntries = direct ? 0 : peer->ntries;
    peer->nprobes = 0;
    return 0;
}

static
int vpeer_update(struct vpeer* peer, vnodeInfo* nodei, time_t rcv_ts, int direct)
{
    int ret = 0;
    vassert(peer);
    vassert(nodei);

    if (direct) {
        peer->rcv_ts = rcv_ts;
    }
    ret = vnodeInfo_update(peer->nodei, nodei);
    retE((ret < 0));

    peer->nprobes = (ret > 0) ? 0 : peer->nprobes;
    peer->ntries  = direct ? 0 : peer->ntries;
    return ret;
}

static
void vpeer_dump(struct vpeer* peer)
{
    vassert(peer);

    vnodeInfo_dump(peer->nodei);
    printf("timestamp[snd]: %s",  peer->snd_ts ? ctime(&peer->snd_ts): "not yet ");
    printf("timestamp[rcv]: %s",  ctime(&peer->rcv_ts));
    printf("tried send times:%d ", peer->ntries);
    printf("probed times:%d", peer->nprobes);
    return ;
}

static
int _aux_space_add_node_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    varg_decl(cookie, 0, vnodeInfo*, nodei);
    varg_decl(cookie, 1, int*,       min_weight);
    varg_decl(cookie, 2, int*,       max_period);
    varg_decl(cookie, 3, time_t*,    now);
    varg_decl(cookie, 4, int*,       found);
    varg_decl(cookie, 5, struct vpeer**, to);

    if (vtoken_equal(&peer->nodei->id, &nodei->id)) {
        *to = peer;
        *found = 1;
        return 1;
    }
    if ((*now - peer->rcv_ts) > *max_period) {
        *to = peer;
        *max_period = *now - peer->rcv_ts;
    }
    if (peer->nodei->weight < *min_weight) {
        *to = peer;
        *min_weight = peer->nodei->weight;
    }
    return 0;
}

static
int _aux_space_probe_node_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, vnodeId*, targetId);
    struct vroute* route = space->route;
    struct vpeer*  peer  = (struct vpeer*)item;
    int ret = 0;

    vassert(space);
    vassert(targetId);
    vassert(peer);

    if (peer->ntries >=  space->max_snd_tms) {
        return 0;
    }
    if (vtoken_equal(&peer->nodei->ver, vnodeVer_unknown())) {
        return 0;
    }
    ret = route->dht_ops->find_node(route, &peer->conn, targetId);
    retE((ret < 0));
    return 0;
}

static
int _aux_space_air_service_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, vsrvcInfo*, srvci);
    struct vpeer* peer = (struct vpeer*)item;
    struct vroute* route = space->route;
    int ret = 0;

    vassert(space);
    vassert(srvci);
    vassert(peer);

    if (peer->ntries >= space->max_snd_tms) {
        return 0; //unreachable.
    }
    if (vtoken_equal(&peer->nodei->ver, vnodeVer_unknown())) {
        return 0;
    }
    ret = route->dht_ops->post_service(route, &peer->conn, srvci);
    retE((ret < 0));
    return 0;
}

static
int _aux_space_probe_service_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, vsrvcHash*, hash);
    struct vpeer*  peer  = (struct vpeer*)item;
    struct vroute* route = space->route;
    int ret = 0;

    vassert(space);
    vassert(hash);
    vassert(peer);

    if (peer->ntries >= space->max_snd_tms) {
        return 0; // unreachable.
    }
    if (vtoken_equal(&peer->nodei->ver, vnodeVer_unknown())) {
        return 0;
    }
    ret = route->dht_ops->find_service(route, &peer->conn, hash);
    retE((ret < 0));
    return 0;
}

static
int _aux_space_reflex_addr_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, struct sockaddr_in*, addr);
    struct vpeer*  peer  = (struct vpeer*)item;
    struct vroute* route = space->route;
    vnodeConn conn;
    int ret = 0;

    vassert(space);
    vassert(addr);
    vassert(peer);

    if (peer->ntries >= space->max_snd_tms) {
        return 0;
    }
    if (vtoken_equal(&peer->nodei->ver, vnodeVer_unknown())) {
        return 0;
    }
    if (!vsockaddr_is_public(&peer->conn.remote)) {
        return 0;
    }
    vnodeConn_set(&conn, addr, &peer->conn.remote);
    ret = route->dht_ops->reflex(route, &conn);
    retE((ret < 0));
    return 0;
}

static
int _aux_space_probe_connectivity_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, struct sockaddr_in*, laddr);
    struct vpeer*  peer  = (struct vpeer*)item;
    struct vroute* route = space->route;
    int i = 0;
    int j = 0;

    vassert(peer);
    vassert(space);
    vassert(laddr);

    if (peer->nprobes >= 3) { //already probed enough;
        return 0;
    }
    if (peer->ntries >= space->max_snd_tms) {
        return 0;
    }
    if (vtoken_equal(&peer->nodei->ver, vnodeVer_unknown())) {
        return 0;
    }
    for (j = 0; j < peer->nodei->naddrs; j++) {
        vnodeConn conn;
        vnodeConn_set(&conn, laddr, &peer->nodei->addrs[i]);
        route->dht_ops->probe(route, &conn, &peer->nodei->id);
    }
    peer->nprobes++;
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
        route->dht_ops->ping(route, &peer->conn);
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
    struct vroute_node_space* node_space = (struct vroute_node_space*)priv;
    char* sid    = (char*)((void**)value)[1];
    char* sver   = (char*)((void**)value)[2];
    char* saddrs = (char*)((void**)value)[3];
    vnodeInfo_relax nodei_relax;
    vnodeInfo* nodei = (vnodeInfo*)&nodei_relax;
    struct sockaddr_in addr;
    char saddr[64];
    vnodeId  id;
    vnodeVer ver;
    char* pos = NULL;

    {
        vtoken_unstrlize(sid, &id);
        vnodeVer_unstrlize(sver, &ver);
        vnodeInfo_relax_init(&nodei_relax, &id, &ver, 0);
        pos = strchr(saddrs, ',');
        while (pos) {
            memset(saddr, 0, 64);
            strncpy(saddr, saddrs, pos - saddrs);
            saddrs = pos + 1;
            vsockaddr_unstrlize(saddr, &addr);
            vnodeInfo_add_addr(&nodei, &addr);
            pos = strchr(saddrs, ',');
        }
        vsockaddr_unstrlize(saddrs, &addr);
        vnodeInfo_add_addr(&nodei, &addr);
    }
    node_space->ops->add_node(node_space, nodei, 0);
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
    char addr[64];
    char addrs[512];
    int  off = 0;
    int  ret = 0;
    int  i = 0;

    vassert(peer);
    vassert(db);

    {
        memset(id,    0, 64);
        memset(ver,   0, 64);
        memset(addr,  0, 64);
        memset(addrs, 0, 512);

        vtoken_strlize   (&peer->nodei->id,  id,  64);
        vnodeVer_strlize (&peer->nodei->ver, ver, 64);
        vsockaddr_strlize(&peer->nodei->addrs[0], addr, 64);
        off += sprintf(addrs + off, "%s", addr);
        for (i = 1; i < peer->nodei->naddrs; i++) {
            memset(addr, 0, 64);
            vsockaddr_strlize(&peer->nodei->addrs[i], addr, 64);
            off += sprintf(addrs + off, ",%s", addr);
        }
    }

    memset(sql_buf, 0, BUF_SZ);
    off += sprintf(sql_buf + off, "insert into '%s' ", VPEER_TB);
    off += sprintf(sql_buf + off, " ('nodeId', 'ver', 'addrs')");
    off += sprintf(sql_buf + off, " values (");
    off += sprintf(sql_buf + off, " '%s',", id   );
    off += sprintf(sql_buf + off, " '%s',", ver  );
    off += sprintf(sql_buf + off, " '%s')", addrs);

    ret = sqlite3_exec(db, sql_buf, 0, 0, &err);
    vlogEv((ret && err), "db err:%s\n", err);
    vlogEv((ret), elog_sqlite3_exec);
    retE((ret));
    return 0;
}

static
int _aux_space_weight_cmp_cb(void* item, void* new, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    struct vpeer* tgt  = (struct vpeer*)new;
    vnodeId* targetId  = (vnodeId*)cookie;
    vnodeMetric pm, tm;

    if (tgt->ntries > 0) {
        // try to not use node that may be unreachable.
        return -1;
    }
    if (!vtoken_equal(&tgt->nodei->ver, &peer->nodei->ver)) {
        // if mismatch for version, try not use the node.
        return -1;
    }
    if (tgt->nodei->weight > peer->nodei->weight) {
        // prefer to use node with hight weight
        return 1;
    }

    vnodeId_dist(&peer->nodei->id, targetId, &pm);
    vnodeId_dist(&tgt->nodei->id,  targetId, &tm);
    return vnodeMetric_cmp(&tm, &pm);
}

/*
 * the routine to add a node to routing table.
 * @route: routing table.
 * @node:  node address to add to routing table.
 * @flags: properties that node has.
 */
static
int _vroute_node_space_add_node(struct vroute_node_space* space, vnodeInfo* nodei, int direct)
{
    struct varray* peers = NULL;
    struct vpeer*  to    = NULL;
    time_t now = time(NULL);
    int min_weight = 0;
    int max_period = 0;
    int found = 0;
    int updt  = 0;
    int idx = 0;
    int ret = 0;

    vassert(space);
    vassert(nodei);
    retS((vtoken_equal(&space->myid, &nodei->id)));

    if (vtoken_equal(&space->myver, &nodei->ver)) {
        nodei->weight++;
    }

    min_weight = nodei->weight;
    idx = vnodeId_bucket(&space->myid, &nodei->id);
    peers = &space->bucket[idx].peers;
    {
        void* argv[] = {
            nodei,
            &min_weight,
            &max_period,
            &now,
            &found,
            &to
        };
        varray_iterate(peers, _aux_space_add_node_cb, argv);
        if (found) { //found
            ret = vpeer_update(to, nodei, now, direct);
            updt = (ret > 0);
        } else if (to && (varray_size(peers) >= space->bucket_sz)) { //replace worst one.
            ret = vpeer_init(to, &space->zaddr, nodei, now, direct);
            updt = (ret >= 0);
        } else if (varray_size(peers) < space->bucket_sz) {
            // insert new one.
            to = vpeer_alloc();
            vlogEv((!to), elog_vpeer_alloc);
            retE((!to));
            vpeer_init(to, &space->zaddr, nodei, now, direct);
            varray_add_tail(peers, to);
            updt = 1;
        } else {
            //bucket is full, discard new
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
int _vroute_node_space_get_node(struct vroute_node_space* space, vnodeId* targetId, vnodeInfo* nodei)
{
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    int found = 0;
    int idx = 0;
    int i = 0;

    vassert(space);
    vassert(targetId);
    vassert(nodei);

    idx = vnodeId_bucket(&space->myid, targetId);
    peers = &space->bucket[idx].peers;
    for (i = 0; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vtoken_equal(&peer->nodei->id, targetId)) {
            memset(nodei, 0, sizeof(vnodeInfo_relax));
            nodei->capc = VNODEINFO_MAX_ADDRS;
            vnodeInfo_copy(nodei, peer->nodei);
            if (vtoken_equal(&peer->nodei->ver, &space->myver)) {
                //minus because uncareness of version as to other nodes.
                nodei->weight -= 1;
            }
            found = 1;
            break;
        }
    }
    return found;
}

/*
 *
 */
static
int _vroute_node_space_get_neighbors(struct vroute_node_space* space, vnodeId* targetId, struct varray* closest, int num)
{
    struct vsorted_array sarray;
    int i = 0;
    int j = 0;

    vassert(space);
    vassert(targetId);
    vassert(closest);
    vassert(num > 0);

    vsorted_array_init(&sarray, 0, _aux_space_weight_cmp_cb, targetId);

    for (i = 0; i < NBUCKETS; i++) {
        struct varray* peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            struct vpeer* peer = (struct vpeer*)varray_get(peers, j);
            if (vtoken_equal(&peer->nodei->id, targetId)) {
                continue;
            }
            if (vtoken_equal(&peer->nodei->ver, vnodeVer_unknown())) {
                continue;
            }
            if (peer->ntries >= space->max_snd_tms) {
                continue;
            }
            vsorted_array_add(&sarray, peer);
        }
    }
    for (i = 0; i < vsorted_array_size(&sarray); i++) {
        struct vpeer* item = (struct vpeer*)vsorted_array_get(&sarray, i);
        vnodeInfo* nodei = NULL;

        if (i >= num) {
            break;
        }
        nodei = (vnodeInfo*)vnodeInfo_relax_alloc();
        if ((!nodei)) {
            break;
        }
        vnodeInfo_copy(nodei, item->nodei);
        if (vtoken_equal(&nodei->ver, &space->myver)) {
            nodei->weight--;
        }
        varray_add_tail(closest, nodei);
    }
    vsorted_array_deinit(&sarray);
    return i;
}

/*
 * the routine to probe a node with given ID
 * @space:
 * @targetId:
 */
static
int _vroute_node_space_probe_node(struct vroute_node_space* space, vnodeId* targetId)
{
    int i = 0;

    vassert(space);
    vassert(targetId);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            targetId
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_probe_node_cb, argv);
    }
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
int _vroute_node_space_air_service(struct vroute_node_space* space, void* srvci)
{
    int i = 0;
    vassert(space);
    vassert(srvci);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            srvci
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_air_service_cb, argv);
    }
    return 0;
}

static
int _vroute_node_space_probe_service(struct vroute_node_space* space, vsrvcHash* hash)
{
    int i = 0;
    vassert(space);
    vassert(hash);

    for (i = 0;  i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            hash
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_probe_service_cb, argv);
    }
    return 0;
}

static
int _vroute_node_space_reflex_addr(struct vroute_node_space* space, struct sockaddr_in* addr)
{
    int i = 0;
    vassert(space);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            addr
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_reflex_addr_cb, argv);
    }
    return 0;
}

static
int _vroute_node_space_adjust_connectivity(struct vroute_node_space* space, vnodeId* targetId, vnodeConn* conn)
{
    struct varray* peers = NULL;
    struct vpeer*  peer  = NULL;
    int idx = 0;
    int i = 0;

    vassert(space);
    vassert(conn);

    idx = vnodeId_bucket(&space->myid, targetId);
    peers = &space->bucket[idx].peers;

    for (i = 0; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vtoken_equal(&peer->nodei->id, targetId)) {
            vnodeConn_adjust(&peer->conn, conn);
            break;
        }
    }
    return 0;
}

static
int _vroute_node_space_probe_connectivity(struct vroute_node_space* space, struct sockaddr_in* laddr)
{
    struct varray* peers = NULL;
    int i = 0;

    vassert(space);
    vassert(laddr);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            laddr
        };
        peers = &space->bucket[i].peers;
        varray_iterate(peers, _aux_space_probe_connectivity_cb, argv);
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
        route->dht_ops->find_closest_nodes(route, &peer->conn, &space->myid);
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
    vlogEv((ret), elog_sqlite3_open);
    retE((ret));

    memset(sql_buf, 0, BUF_SZ);
    sprintf(sql_buf, "select * from %s", VPEER_TB);
    ret = sqlite3_exec(db, sql_buf, _aux_space_load_cb, space, &err);
    vlogEv((ret && err), "db err:%s\n", err);
    vlogEv((ret), elog_sqlite3_exec);
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
    vlogEv((ret), elog_sqlite3_open);
    retE((ret));

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        varray_iterate(peers, _aux_space_store_cb, db);
    }
    sqlite3_close(db);
    vlogI("writeback route infos");
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

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        while(varray_size(peers) > 0) {
            vpeer_free((struct vpeer*)varray_pop_tail(peers));
        }
    }
    return ;
}

/*
 * to iterate all node infos in routing table.
 */
static
void _vroute_node_space_iterate(struct vroute_node_space* space, vroute_node_space_iterate_t cb, void* cookie, vtoken* token)
{
    struct varray* peers = NULL;
    int i = 0;
    int j = 0;

    vassert(space);

    if (!cb) {
        return;
    }

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            cb((struct vpeer*)varray_get(peers, j), cookie, token);
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
    int titled = 0;
    int i = 0;
    int j = 0;
    vassert(space);

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            if (!titled) {
                vdump(printf("-> list of peers in node routing space:"));
                titled = 1;
            }
            printf("{ ");
            vpeer_dump((struct vpeer*)varray_get(peers, j));
            printf(" }\n");
        }
    }

    return ;
}

struct vroute_node_space_ops route_space_ops = {
    .add_node      = _vroute_node_space_add_node,
    .get_node      = _vroute_node_space_get_node,
    .get_neighbors = _vroute_node_space_get_neighbors,
    .probe_node    = _vroute_node_space_probe_node,
    .air_service   = _vroute_node_space_air_service,
    .probe_service = _vroute_node_space_probe_service,
    .reflex_addr   = _vroute_node_space_reflex_addr,
    .adjust_connectivity = _vroute_node_space_adjust_connectivity,
    .probe_connectivity  = _vroute_node_space_probe_connectivity,
    .tick          = _vroute_node_space_tick,
    .load          = _vroute_node_space_load,
    .store         = _vroute_node_space_store,
    .clear         = _vroute_node_space_clear,
    .iterate       = _vroute_node_space_iterate,
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
    vlogEv((ret), elog_sqlite3_open);
    retE((ret));

    memset(sql_buf, 0, BUF_SZ);
    ret += sprintf(sql_buf + ret, "CREATE TABLE '%s' (", VPEER_TB);
    ret += sprintf(sql_buf + ret, "'id' INTEGER PRIOMARY_KEY,");
    ret += sprintf(sql_buf + ret, "'nodeId' TEXT,");
    ret += sprintf(sql_buf + ret, "'ver'    TEXT,");
    ret += sprintf(sql_buf + ret, "'addrs'  TEXT)");

    ret = sqlite3_exec(db, sql_buf, NULL, NULL, &err);
    vlogEv((ret && err), "db err:%s\n", err);
    vlogEv((ret), elog_sqlite3_exec);
    sqlite3_close(db);
    return 0;
}

int vroute_node_space_init(struct vroute_node_space* space, struct vroute* route, struct vconfig* cfg, vnodeId* myid)
{
    vnodeVer myver;
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(cfg);
    vassert(myid);

    strncpy(space->db, cfg->ext_ops->get_route_db_file(cfg), BUF_SZ);
    vsockaddr_convert2(INADDR_ANY, cfg->ext_ops->get_dht_port(cfg), &space->zaddr);

    space->bucket_sz   = cfg->ext_ops->get_route_bucket_sz(cfg);
    space->max_snd_tms = cfg->ext_ops->get_route_max_snd_tms(cfg);
    space->max_rcv_tmo = cfg->ext_ops->get_route_max_rcv_tmo(cfg);

    ret = _aux_space_prepare_db(space);
    retE((ret < 0));

    for (i = 0; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].peers, 8);
        space->bucket[i].ts = 0;
    }

    vnodeVer_unstrlize(vhost_get_version(), &myver);
    vtoken_copy(&space->myver, &myver);
    vtoken_copy(&space->myid,  myid);
    space->route = route;
    space->ops   = &route_space_ops;
    return 0;
}

void vroute_node_space_deinit(struct vroute_node_space* space)
{
    int i = 0;
    vassert(space);

    space->ops->clear(space);
    for (i = 0; i < NBUCKETS; i++) {
        varray_deinit(&space->bucket[i].peers);
    }
    return ;
}

