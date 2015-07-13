#include "vglobal.h"
#include "vroute.h"

#define VPEER_TB ((const char*)"dht_peer")
#define vminimum(x, y) (x >= y ? y : x)
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
int vpeer_init(struct vpeer* peer, struct vroute_node_space* space, vnodeInfo* nodei)
{
    vassert(peer);
    vassert(space);
    vassert(nodei);

    vnodeInfo_copy(&peer->nodei, nodei);
    vnodeConn_set (&peer->conn, &space->zaddr, vnodeInfo_worst_addr(peer->nodei));

    peer->tick_ts     = time(NULL);
    peer->next_tmo    = space->init_next_tmo;
    peer->ntry_pings  = 0;
    peer->ntry_probes = 0;
    return 0;
}

static
int vpeer_update(struct vpeer* peer, struct vroute_node_space* space, vnodeInfo* nodei, int flag)
{
    int ret = 0;
    vassert(peer);
    vassert(space);
    vassert(nodei);

    switch(flag) {
    case VADD_BY_PING_RSP:
        /*
         * If received a DHT ping response from bad node, then that bad node
         * turned to be good node with initial 'next_tmo';
         * If received a DHT ping response from good node, then 'next_tmo' of
         * good node can be prolonged with a small step.
         *
         */
        if (peer->ntry_pings > 2) { //means bad node
            peer->next_tmo   = space->init_next_tmo;
        } else {
            peer->next_tmo++;
            peer->next_tmo = vminimum(peer->next_tmo, space->max_next_tmo >> 1);
        }
        peer->ntry_pings = 0;
        peer->tick_ts = time(NULL);
        break;
    case VADD_BY_PING:
        /*
         * If received a DHT ping query from bad node, then regress 'next_tmo'
         * back to initial value, so as to send ping as soon as possible.
         * If received a DHT ping query from good node, do nothing but update
         * 'tick_ts'.
         */
        if (peer->ntry_pings > 1) {
            peer->next_tmo = space->init_next_tmo;
        }
        peer->tick_ts = time(NULL);
        break;
    case VADD_BY_OTHER:
        /* updated not because of the DHT node itself. So, no sure for this
         * DHT node is bad or good for now.
         */
        //do nothing;
        break;
    default:
        vassert(0);
        break;
    }

    ret = vnodeInfo_merge(&peer->nodei, nodei);
    retE((ret < 0));
    if (ret > 0) {
        vnodeConn_set(&peer->conn, &space->zaddr, vnodeInfo_worst_addr(peer->nodei));
        peer->ntry_probes = 0;
    }
    return ret;
}

static
void vpeer_dump(struct vpeer* peer)
{
    vassert(peer);

    vnodeInfo_dump(peer->nodei);
    printf("local:");    vsockaddr_dump(&peer->conn.local);
    printf(" remote: "); vsockaddr_dump(&peer->conn.remote); printf("\n");
    printf("tick ts: %s", ctime(&peer->tick_ts));
    printf("next_tmo:%d\n", peer->next_tmo);
    printf("tried ping  times:%d\n", peer->ntry_pings);
    printf("tried probe times:%d\n", peer->ntry_probes);
    return ;
}

static
int _aux_space_add_node_cb(void* item, void* cookie)
{
    struct vpeer* peer = (struct vpeer*)item;
    varg_decl(cookie, 0, vnodeInfo*, nodei);
    varg_decl(cookie, 1, int*,       min_wgt);
    varg_decl(cookie, 2, int*,       max_try);
    varg_decl(cookie, 3, int*,       found);
    varg_decl(cookie, 4, struct vpeer**, want);

    if (vtoken_equal(&peer->nodei->id, &nodei->id)) {
        *want  = peer;
        *found = 1;
        return 1;
    }

    if (*max_try < peer->ntry_pings) {
        *want = peer;
        *max_try = peer->ntry_pings;
        return 0;
    }
    if (*min_wgt > peer->nodei->weight) {
        *want = peer;
        *min_wgt = peer->nodei->weight;
        return 0;
    }
    return 0;
}

static
int _aux_space_find_node_in_neighbors_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, vnodeId*, targetId);
    struct vroute* route = space->route;
    struct vpeer*  peer  = (struct vpeer*)item;
    int ret = 0;

    vassert(space);
    vassert(targetId);
    vassert(peer);

    if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
        //means it's a 'worst' bad DHT node (unreachable);
        return 0;
    }
    if (vnodeInfo_is_fake(peer->nodei)) {
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
    struct vpeer* peer   = (struct vpeer*)item;
    struct vroute* route = space->route;
    int ret = 0;

    vassert(space);
    vassert(srvci);
    vassert(peer);

    if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
        //means it's a 'worst' bad DHT node (unreachable);
        return 0;
    }
    if (vnodeInfo_is_fake(peer->nodei)) {
        //Do not air service to fake DHT node.
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

    if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
        //means it's a 'worst' bad DHT node (unreachable);
        return 0;
    }
    if (vnodeInfo_is_fake(peer->nodei)) {
        //Do not air service to fake DHT node.
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

    if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
        //means it's a 'worst' bad DHT node (unreachable);
        return 0;
    }
    //if (vtoken_equal(&space->nodei->ver, vnodeVer_unknown())) {
        //Do not air service to fake DHT node.
    //    return 0;
    //}
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

    if (peer->ntry_probes >= space->max_probe_tms) {
        //already probed enough times;
        return 0;
    }
    if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
        //means it's a 'worst' bad DHT node (unreachable);
        return 0;
    }
    if (vnodeInfo_is_fake(peer->nodei)) {
        //Do not probe connectivity with fake DHT node.
        return 0;
    }
    for (j = 0; j < peer->nodei->naddrs; j++) {
        vnodeConn conn;
        vnodeConn_set(&conn, laddr, &peer->nodei->addrs[i].addr);
        route->dht_ops->probe(route, &conn, &peer->nodei->id);
    }
    peer->ntry_probes++;
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

    /*
     * each peer has it's own 'next_tmo' value, which means the interval
     * time to send 'ping' query to that node for next time.
     */
    if (peer->tick_ts + peer->next_tmo > *now) {
        return 0;
    }
    if ((peer->ntry_pings > space->max_ping_tms) && vnodeInfo_is_fake(peer->nodei)) {
        return 0;
    }

    route->dht_ops->ping(route, &peer->conn);
    peer->ntry_pings++;
    peer->tick_ts = time(NULL);

    if (peer->ntry_pings > space->max_ping_tms) {
        /* if no rsp to DHT ping query is received from that bad node for
         * more than certian times, then increase 'next_tmo', so as to
         * reduce frequency of sending DHT 'ping' query.
         */
        peer->next_tmo += 2;
        peer->next_tmo = vminimum(peer->next_tmo, space->max_next_tmo);
    }
    return 0;
}

static
int _aux_vsockaddr_unstrlize(const char* buf, struct vsockaddr_in* addr)
{
    char* cur = NULL;
    char* end = NULL;
    char ip[64];
    int  port = 0;
    int  type = 0;
    int  ret = 0;

    vassert(buf);
    vassert(addr);

    cur = strchr(buf, ':');
    retE((!cur));

    memset(ip, 0, 64);
    strncpy(ip, buf, cur-buf);

    cur += 1;
    errno = 0;
    port = strtol(cur, &end, 10);
    vlogEv((errno), elog_strtol);
    retE((errno));

    ret = vsockaddr_convert(ip, port, &addr->addr);
    vlogEv((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));

    end = strchr(end, ':');
    retE((!end));
    end += 1;
    errno = 0;
    type = strtol(end, NULL, 10);
    vlogEv((errno), elog_strtol);
    retE((errno));

    addr->type = (uint32_t)type;
    return 0;
}

/*
 *
 */
static
int _aux_space_load_cb(void* priv, int col, char** value, char** field)
{
    struct vroute_node_space* node_space = (struct vroute_node_space*)priv;
    char* sid    = (char*)((void**)value)[0];
    char* sver   = (char*)((void**)value)[1];
    char* saddrs = (char*)((void**)value)[2];
    DECL_VNODE_RELAX(nodei);
    struct vsockaddr_in vaddr;
    char saddr[64];
    vnodeId  id;
    vnodeVer ver;
    char* pos = NULL;

    {
        vtoken_unstrlize(sid, &id);
        vnodeVer_unstrlize(sver, &ver);
        vnodeInfo_relax_init(nodei, &id, &ver, 0);
        pos = strchr(saddrs, ',');
        while (pos) {
            memset(saddr, 0, 64);
            strncpy(saddr, saddrs, pos - saddrs);
            saddrs = pos + 1;
            _aux_vsockaddr_unstrlize(saddr, &vaddr);
            vnodeInfo_add_addr(&nodei, &vaddr.addr, vaddr.type);
            pos = strchr(saddrs, ',');
        }
        _aux_vsockaddr_unstrlize(saddrs, &vaddr);
        vnodeInfo_add_addr(&nodei, &vaddr.addr, vaddr.type);
    }
    node_space->ops->add_node(node_space, nodei, VADD_BY_OTHER);
    return 0;
}

static
int _aux_space_store_cb(void* item, void* cookie)
{
    varg_decl(cookie, 0, struct vroute_node_space*, space);
    varg_decl(cookie, 1, sqlite3*, db);
    struct vpeer* peer = (struct vpeer*)item;
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

    if (vnodeInfo_is_fake(peer->nodei)) {
        //skip fake DHT node.
        return 0;
    }
    if (peer->ntry_pings > 0 && peer->next_tmo >= space->max_next_tmo){
        //skip 'worst' bad DHT node.
        return 0;
    }
    {
        memset(id,    0, 64);
        memset(ver,   0, 64);
        memset(addr,  0, 64);
        memset(addrs, 0, 512);

        vtoken_strlize   (&peer->nodei->id,  id,  64);
        vnodeVer_strlize (&peer->nodei->ver, ver, 64);
        vsockaddr_strlize(&peer->nodei->addrs[0].addr, addr, 64);
        sprintf(addr + strlen(addr), ":%u", peer->nodei->addrs[0].type);
        off = 0;
        off += sprintf(addrs + off, "%s", addr);
        for (i = 1; i < peer->nodei->naddrs; i++) {
            memset(addr, 0, 64);
            vsockaddr_strlize(&peer->nodei->addrs[i].addr, addr, 64);
            sprintf(addr + strlen(addr), ":%u", peer->nodei->addrs[i].type);
            off += sprintf(addrs + off, ",%s", addr);
        }
    }

    memset(sql_buf, 0, BUF_SZ);
    off = 0;
    off += sprintf(sql_buf + off, "insert or replace into '%s' ", VPEER_TB);
    off += sprintf(sql_buf + off, " ('id', 'ver', 'addrs')");
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
    struct vpeer* peer   = (struct vpeer*)item;
    struct vpeer* target = (struct vpeer*)new;
    vnodeId* targetId = (vnodeId*)cookie;
    vnodeMetric pm, tm;

    if (target->ntry_pings > peer->ntry_pings) {
        return -1;
    }
    if (!vtoken_equal(&target->nodei->ver, &peer->nodei->ver)) {
        return -1;
    }
    if (target->nodei->weight > peer->nodei->weight) {
        return 1;
    }
    vnodeId_dist(&peer->nodei->id, targetId, &pm);
    vnodeId_dist(&target->nodei->id,  targetId, &tm);
    return vnodeMetric_cmp(&tm, &pm);
}

static
void _vroute_node_space_kick_node(struct vroute_node_space* space, vnodeId* id)
{
    struct varray* peers = NULL;
    int idx = 0;
    int i = 0;
    vassert(space);
    vassert(id);

    idx = vnodeId_bucket(&space->myid, id);
    peers = &space->bucket[idx].peers;
    for (i = 0; i < varray_size(peers); i++) {
        struct vpeer* peer = (struct vpeer*)varray_get(peers, i);
        if (!vtoken_equal(&peer->nodei->id, id)) {
            continue;
        }
        /*
         * All dht messages from 'peer' are considered as ping-like messages.
         * So, when message is received from that 'peer', 'rcv_ts' to that
         * 'peer' will be updated.
         */
        peer->tick_ts = time(NULL);
        break;
    }
    return ;
}

/*
 * add a DHT node to node routing space.
 *
 * @route: routing table.
 * @nodei: nodeinfo to add;
 * @flag:  flag to indicate 3 following situation:
 *          0: occured when 'nodei' is not from it's DHT node, such as by 'find_
 *             node' or 'find_closest_nodes_rsp' message.
 *          1: occured when received 'ping' query;
 *          2: occured when received a response to ping query.
 */
static
int _vroute_node_space_add_node(struct vroute_node_space* space, vnodeInfo* nodei, int flag)
{
    struct varray* peers = NULL;
    struct vpeer*  want  = NULL;
    int updated = 0;
    int min_wgt = 0;
    int max_try = 0;
    int found = 0;
    int idx = 0;
    int ret = 0;

    vassert(space);
    vassert(nodei);
    if (vtoken_equal(&space->myid, &nodei->id)) {
        return 0;
    }
    if (vtoken_equal(&space->myver, &nodei->ver)) {
        nodei->weight++;
    }
    min_wgt = nodei->weight;
    idx = vnodeId_bucket(&space->myid, &nodei->id);
    peers = &space->bucket[idx].peers;
    {
        void* argv[] = {
            nodei,
            &min_wgt,
            &max_try,
            &found,
            &want,
        };
        varray_iterate(peers, _aux_space_add_node_cb, argv);
        if (found) { //found
            ret = vpeer_update(want, space, nodei, flag);
            updated = (ret > 0);
        } else if (want && varray_size(peers) >= space->bucket_sz) {
            //replace worst 'bad' nodei;
            ret = vpeer_update(want, space, nodei, flag);
            updated = (ret > 0);
        } else if (varray_size(peers) < space->bucket_sz) {
            want = vpeer_alloc();
            vlogEv((!want), elog_vpeer_alloc);
            retE((!want));
            vpeer_init(want, space, nodei);
            varray_add_tail(peers, want);
            updated = 1;
        } else {
            //bucket is full, discard new
        }
        if (updated) {
            space->bucket[idx].ts = time(NULL);
        }
    }
    return 0;
}

/*
 * the routine to find a node with given nodeID @targetId in local node routing space.
 *
 * @route: handle to route table.
 * @targetId: node Id to find
 * @nodei[out]:
 */
static
int _vroute_node_space_find_node(struct vroute_node_space* space, vnodeId* targetId, vnodeInfo* nodei)
{
    struct varray* peers = NULL;
    int found = 0;
    int idx = 0;
    int i   = 0;

    vassert(space);
    vassert(targetId);
    vassert(nodei);

    idx = vnodeId_bucket(&space->myid, targetId);
    peers = &space->bucket[idx].peers;
    for (i = 0; i < varray_size(peers); i++) {
        struct vpeer* peer = (struct vpeer*)varray_get(peers, i);
        if (!vtoken_equal(&peer->nodei->id, targetId)) {
            continue;
        }
        vnodeInfo_copy(&nodei, peer->nodei);
        if (vtoken_equal(&peer->nodei->ver, &space->myver)) {
            nodei->weight--;
        }
        found = 1;
        break;
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
            if (vnodeInfo_is_fake(peer->nodei)) {
                continue;
            }
            if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
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
        nodei = vnodeInfo_relax_alloc();
        if ((!nodei)) {
            break;
        }
        vnodeInfo_copy(&nodei, item->nodei);
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
int _vroute_node_space_find_node_in_neighbors(struct vroute_node_space* space, vnodeId* targetId)
{
    int i = 0;
    vassert(space);
    vassert(targetId);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            targetId
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_find_node_in_neighbors_cb, argv);
    }
    return 0;
}

/*
 *  the routine to broadcast the give service @svci to all nodes in routing
 *  table. which is provied by local node.
 *
 * @space:
 * @svci:  metadata of service.
 *
 */
static
int _vroute_node_space_air_service(struct vroute_node_space* space, vsrvcInfo* srvci)
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

/*
 * the routine to find service in neighbor nodes
 *
 * @space:
 * @hash:  the service hash ID to find.
 *
 */
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

/*
 * the routine to get reflexive address from remote node in internet.
 *
 * @space:
 * @addr: the local address to get it's reflexive address.
 *
 */
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

/*
 * the routine to select better candidate connection.
 *
 * @space:
 * @targetId:
 * @conn: the candidate connection.
 */
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

/*
 * the routine to probe connectivity with all neighbor nodes.
 *
 * @space:
 * @laddr: the source address where the probe query is from
 */
static
int _vroute_node_space_probe_connectivity(struct vroute_node_space* space, struct sockaddr_in* laddr)
{
    int i = 0;
    vassert(space);
    vassert(laddr);

    for (i = 0; i < NBUCKETS; i++) {
        void* argv[] = {
            space,
            laddr
        };
        varray_iterate(&space->bucket[i].peers, _aux_space_probe_connectivity_cb, argv);
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
        if (space->bucket[i].ts + space->max_next_tmo > now) {
            continue;
        }
        peer = (struct vpeer*)varray_get_rand(peers);
        if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
            continue;
        }
        if (vnodeInfo_is_fake(peer->nodei)) {
            continue;
        }

        route->dht_ops->find_closest_nodes(route, &peer->conn, &space->myid);
        space->bucket[i].ts = time(NULL);
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
    sprintf(sql_buf, "select * from '%s'", VPEER_TB);
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
        void* argv[] = {
            space,
            db
        };
        peers = &space->bucket[i].peers;
        varray_iterate(peers, _aux_space_store_cb, argv);
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
 * to inspect node routing space. used for testing module.
 *
 * @space:
 * @cb: inspect callback
 * @cookie: private data to @cb;
 * @token: token at that moment;
 * @insp_id: type ID of inspection;
 */
static
void _vroute_node_space_inspect(struct vroute_node_space* space, vroute_node_space_inspect_t cb, void* cookie, vtoken* token, uint32_t insp_id)
{
    struct varray* peers = NULL;
    int i = 0;
    int j = 0;

    vassert(space);
    vassert(cb);

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            cb((struct vpeer*)varray_get(peers, j), cookie, token, insp_id);
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
    struct vpeer*  peer  = NULL;
    int titled = 0;
    int i = 0;
    int j = 0;
    vassert(space);

    for (i = 0; i < NBUCKETS; i++) {
        peers = &space->bucket[i].peers;
        for (j = 0; j < varray_size(peers); j++) {
            peer = (struct vpeer*)varray_get(peers, j);
            if ((peer->ntry_pings > 0) && (peer->next_tmo >= space->max_next_tmo)) {
                continue;
            }
            if ((peer->ntry_pings > 0) && vnodeInfo_is_fake(peer->nodei)) {
                continue;
            }
            if (!titled) {
                vdump(printf("-> list of peers in node routing space:"));
                titled = 1;
            }
            printf("{ ");
            vpeer_dump(peer);
            printf(" }\n");
        }
    }

    return ;
}

struct vroute_node_space_ops route_space_ops = {
    .kick_node     = _vroute_node_space_kick_node,
    .add_node      = _vroute_node_space_add_node,
    .find_node     = _vroute_node_space_find_node,
    .find_node_in_neighbors = _vroute_node_space_find_node_in_neighbors,
    .get_neighbors = _vroute_node_space_get_neighbors,
    .air_service   = _vroute_node_space_air_service,
    .probe_service = _vroute_node_space_probe_service,
    .reflex_addr   = _vroute_node_space_reflex_addr,
    .adjust_connectivity = _vroute_node_space_adjust_connectivity,
    .probe_connectivity  = _vroute_node_space_probe_connectivity,
    .tick          = _vroute_node_space_tick,
    .load          = _vroute_node_space_load,
    .store         = _vroute_node_space_store,
    .clear         = _vroute_node_space_clear,
    .inspect       = _vroute_node_space_inspect,
    .dump          = _vroute_node_space_dump
};

static
int _aux_space_prepare_peer_tb(struct vroute_node_space* space)
{
    char sql_buf[BUF_SZ];
    sqlite3* db = NULL;
    char* err = NULL;
    int ret = 0;

    ret = sqlite3_open(space->db, &db);
    vlogEv((ret), elog_sqlite3_open);
    retE((ret));

    memset(sql_buf, 0, BUF_SZ);
    sprintf(sql_buf, "select * from '%s'", VPEER_TB);
    ret = sqlite3_exec(db, sql_buf, NULL, NULL, &err);
    if (!ret) {
        // table is already exist.
        sqlite3_close(db);
        return 0;
    }

    memset(sql_buf, 0, BUF_SZ);
    ret  = sprintf(sql_buf, "CREATE TABLE '%s' (", VPEER_TB);
    ret += sprintf(sql_buf + ret, "'id' char(160) PRIMARY KEY,");
    ret += sprintf(sql_buf + ret, "'ver'    char(160),");
    ret += sprintf(sql_buf + ret, "'addrs'  TEXT)");

    ret = sqlite3_exec(db, sql_buf, NULL, NULL, &err);
    vlogEv((ret), elog_sqlite3_exec);
    vlogEv((ret && err), "prepare db err:%s\n", err);

    sqlite3_close(db);
    return 0;
}

int vroute_node_space_init(struct vroute_node_space* space, struct vroute* route, struct vconfig* cfg, vnodeId* myid)
{
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(cfg);
    vassert(myid);

    strncpy(space->db, cfg->ext_ops->get_host_db_file(cfg), BUF_SZ);
    ret = cfg->ext_ops->get_dht_address(cfg, &space->zaddr);
    retE((ret < 0));

    space->bucket_sz     = cfg->ext_ops->get_route_bucket_sz(cfg);
    space->init_next_tmo = 2;
    space->max_next_tmo  = cfg->ext_ops->get_route_max_next_tmo(cfg);
    space->max_ping_tms  = 3;
    space->max_probe_tms = 3;

    ret = _aux_space_prepare_peer_tb(space);
    retE((ret < 0));

    for (i = 0; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].peers, 8);
        space->bucket[i].ts = 0;
    }

    vnodeVer_unstrlize(vhost_get_version(), &space->myver);
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

