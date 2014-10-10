#include "vglobal.h"
#include "vroute.h"

#define BUF_SZ (1024)
#define VPEER_TB ((const char*)"dht_peer")

#define TDIFF(a, b)   ((int)(a-b))
#define EXPIRED(a, b) (TDIFF(a, b) > MAX_RCV_PERIOD)

#define MAX_CAPC ((int)8)
#define vmin(a, b)   ((a < b) ? a: b)

#define varg_decl(argv, idx, type, var)  type var = (type)argv[idx]
#define type_decl(type, var, src)        type var = (type)src

/*
 * for vpeer
 */
static MEM_AUX_INIT(peer_cache, sizeof(struct vpeer), 0);
struct vpeer* vpeer_alloc(void)
{
    struct vpeer* peer = NULL;

    peer = (struct vpeer*)vmem_aux_alloc(&peer_cache);
    vlog_cond((!peer), elog_vmem_aux_alloc);
    retE_p((!peer));
    return peer;
}
/*
 * @peer:
 */
void vpeer_free(struct vpeer* peer)
{
    vassert(vpeer);
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
void vpeer_init(struct vpeer* peer, vnodeAddr* addr, time_t snd_ts, time_t rcv_ts, int flags)
{
    vassert(peer);
    vassert(addr);
    vassert(snd_ts);
    vassert(rcv_ts);

    vnodeAddr_copy(&peer->extId, addr);
    peer->snd_ts = snd_ts;
    peer->rcv_ts = rcv_ts;
    peer->flags  = flags;
    return ;
}

/*
 *  auxilary function for @route->ops->add
 */
struct vpeer* _aux_get_worst(struct varray* peers, int flags)
{
    int _aux_get_worst_cb(void* item, void* cookie)
    {
        varg_decl(((void**)cookie), 0, int*, max_period);
        varg_decl(((void**)cookie), 1, int*, min_flags );
        varg_decl(((void**)cookie), 2, time_t*, now    );
        varg_decl(((void**)cookie), 3, struct vpeer**,target);
        struct vpeer* peer = (struct vpeer*)item;

        vassert(max_period);
        vassert(min_flags);
        vassert(now);
        vassert(*target);
        vassert(peer);

        if (TDIFF(*now, peer->rcv_ts) > *max_period) {
            *target = peer;
            *max_period = TDIFF(*now, peer->rcv_ts);
        }
        if (peer->flags < *min_flags) {
            *target = peer;
            *min_flags = peer->flags;
        }
        return 0;
    }

    struct vpeer* peer = NULL;
    time_t now = time(NULL);
    int max_period = MAX_RCV_PERIOD;
    int min_flags  = flags;
    void* argv[4]  = {
        &max_period,
        &min_flags,
        &now,
        &peer
    };

    vassert(peers);
    varray_iterate(peers, _aux_get_worst_cb, argv);
    return peer;
}

/*
 * to add a node to routing table.
 * @route: routing table.
 * @node:  node address to be added to routing table.
 * @flags: properties that node has.
 */
static
int _vroute_add(struct vroute* route, vnodeAddr* node, int flags)
{
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    time_t now = time(NULL);
    int updated = 0;
    int idx = 0;
    int i = 0;

    vassert(route);
    vassert(node);

    idx = vnodeId_bucket(&route->ownId.id, &node->id);
    vlock_enter(&route->lock);
    peers = &route->bucket[idx].peers;
    for (; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vnodeAddr_equal(&peer->extId, node)) {
            break;
        }
    }
    if ( i < varray_size(peers)) { // if existed, just modification.
        vpeer_init(peer, node, peer->snd_ts, now, peer->flags | flags);
        updated = 1;
    } else if (varray_size(peers) >= BUCKET_CAPC) { // replace the worst one.
        peer = _aux_get_worst(peers, flags);
        if (peer) {
            vpeer_init(peer, node, 0, now, flags);
            updated = 1;
        }
    } else { // be addable for new one.
        peer = vpeer_alloc();
        vlog_cond((!peer), elog_vpeer_alloc);
        ret2E((!peer), vlock_leave(&route->lock));

        vpeer_init(peer, node, 0, now, flags);
        varray_add_tail(peers, peer);
        updated = 1;
    }
    if (updated) {
        route->bucket[idx].ts = now;
    }
    vlock_leave(&route->lock);
    return 0;
}

/*
 * to remove a node given by vnodeId or sockaddr from routing table.
 * @route:
 * @node:
 *
 */
static
int _vroute_remove(struct vroute* route, vnodeAddr* node)
{
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    int idx = 0;
    int i   = 0;

    vassert(route);
    vassert(node);

    idx = vnodeId_bucket(&route->ownId.id, &node->id);
    vlock_enter(&route->lock);
    peers = &route->bucket[idx].peers;
    for (; i < varray_size(peers); i++) {
        peer = (struct vpeer*)varray_get(peers, i);
        if (vnodeAddr_equal(&peer->extId, node)) {
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
 *
 */
static
int _aux_load_cb(void* priv, int col, char** value, char** field)
{
    varg_decl(((void**)priv ),0, struct vroute*, route);
    varg_decl(((void**)value),0, char*, host	); // for field "host"
    varg_decl(((void**)value),1, int,   port	); // for field "port"
    varg_decl(((void**)value),2, char*, id_str  ); // for filed "node id"
    vnodeAddr extId;

    vassert(route);
    vassert(host);
    vassert(port > 0);
    vassert(id_str);

    vnodeId_unstrlize(id_str, &extId.id);
    vsockaddr_convert(host, port, &extId.addr);
    return _vroute_add(route, &extId, 0);
}

/*
 * to load all nodes info from db file to routing table
 * @route:
 * @file : file name for stored db.
 *
 */
static
int _vroute_load(struct vroute* route, const char* file)
{
    char sql_buf[BUF_SZ];
    sqlite3* db = NULL;
    char* err = NULL;
    int ret = 0;

    vassert(route);
    vassert(file);

    ret = sqlite3_open(file, &db);
    vlog_cond((ret), elog_sqlite3_open);
    retE((ret));

    sprintf(sql_buf, "select * from %s", VPEER_TB);
    ret = sqlite3_exec(db, sql_buf, _aux_load_cb, route, &err);
    vlog_cond((ret && err), printf("db err:%s\n", err));
    sqlite3_close(db);
    vlog_cond((ret), elog_sqlite3_exec);
    retE((ret));

    return 0;
}

static
int _aux_store_item(sqlite3* db, void* item)
{
    struct vpeer* peer = (struct vpeer*)item;
    char sql_buf[BUF_SZ];
    char host[64];
    int  port = 0;
    char id_str[64];
    char* err = NULL;
    int off = 0;
    int ret = 0;

    vassert(db);
    vassert(peer);

    memset(id_str, 0, 64);
    vnodeId_strlize(&peer->extId.id, id_str, 64);
    vsockaddr_unconvert(&peer->extId.addr, host, 64, &port);

    off += sprintf(sql_buf + off, "insert into %s ", VPEER_TB);
    off += sprintf(sql_buf + off, " (%s,", host);
    off += sprintf(sql_buf + off, "  %i,", port);
    off += sprintf(sql_buf + off, "  %s)", id_str);

    ret = sqlite3_exec(db, sql_buf, 0, 0, &err);
    vlog_cond((ret && err), printf("db err:%s\n", err));
    vlog_cond((ret), elog_sqlite3_exec);
    retE((ret));
    return 0;
}

/*
 * to store all nodes info in routing table back to db file.
 * @route:
 * @file
 */
static
int _vroute_store(struct vroute* route, const char* file)
{
    struct varray* peers = NULL;
    sqlite3* db = NULL;
    int ret = 0;
    int i = 0;
    int j = 0;

    vassert(route);
    vassert(file);

    ret = sqlite3_open(file, &db);
    vlog_cond((ret), elog_sqlite3_open);
    retE((ret));

    vlock_enter(&route->lock);
    for (; i < NBUCKETS; i++) {
        peers = &route->bucket[i].peers;
        for (; j < varray_size(peers); j++) {
            _aux_store_item(db, varray_get(peers, j));
        }
    }
    vlock_leave(&route->lock);
    sqlite3_close(db);
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
    vassert(route);
    //todo;
    return 0;
}

static
int _aux_tick_cb(void* item, void* cookie)
{
    varg_decl(((void**)cookie), 0, struct vroute*, route);
    varg_decl(((void**)cookie), 1, time_t*, now);
    type_decl(struct vpeer*, peer, item);
    vassert(peer);
    vassert(now);

    if (PROP_UNREACHABLE== peer->flags) {
        return 0;
    }
    if ((peer->snd_ts)&&
        (peer->ntries = MAX_SND_TIMES)) {
        peer->flags = PROP_UNREACHABLE;
        return 0;
    }

    if ((!peer->snd_ts)||
        (EXPIRED(peer->rcv_ts, *now))) {
        route->dht_ops->ping(route, &peer->extId);
    }
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
        peers = &route->bucket[i].peers;
        varray_iterate(peers, _aux_tick_cb, route);

        if (!EXPIRED(route->bucket[i].ts, now)) {
            continue;
        }
        peer = (struct vpeer*)varray_get_rand(peers);
        route->dht_ops->find_closest_nodes(route, &peer->extId, &route->ownId.id);
    }
    vlock_leave(&route->lock);
    return 0;
}

static
int _vroute_find(struct vroute* route, vnodeId* targetId, vnodeInfo* info)
{
    struct varray* peers = NULL;
    struct vpeer* peer = NULL;
    int found = 0;
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
        if (vnodeId_equal(&route->ownId.id, targetId)) {
            found = 1;
            break;
        }
    }
    if (found) {
        vnodeInfo_init(info, targetId, &peer->extId.addr, peer->flags, &peer->ver);
        info->flags &= ~PROP_VER;
    }
    vlock_leave(&route->lock);
    return (found) ? 0 : -1;
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
 * @route:
 * @dest :  the dest sockaddr of target node.
 */
static
int _out_ping(struct vroute* route, vnodeAddr* dest)
{
    void* _buf = NULL;
    void*  buf = NULL;
    vtoken token;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);

    vtoken_make(&token);
    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8; // reserve padding for magic and msgId.
    len -= 8;

    ret = route->dht_enc_ops->ping(&token, &route->ownId.id, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_PING,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_caches, _buf));
    }
    return 0;
}

/*
 * @route:
 * @from :
 * @info :
 */
static
int _out_ping_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, vnodeInfo* info)
{
    void* _buf = NULL;
    void*  buf = NULL;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(info);

    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8; // reserve padding for magic and msgId.
    len -= 8;

    ret = route->enc_ops->ping_rsp(token, &dest->id, info, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_PING_RSP,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
    }
    return 0;
}

/*
 * @route
 * @dest:
 * @target
 */
static
int _out_find_node(struct vroute* route, vnodeAddr* dest, vnodeId* target)
{
    void* _buf = NULL;
    void*  buf = NULL;
    vtoken token;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(target);

    vtoken_make(&token);
    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8;
    len -= 8;

    ret = route->enc_ops->find_node(&token, &route->ownId.id, target, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_FIND_NODE,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
    }
    return 0;
}

/*
 * @route
 * @
 */
static
int _out_find_node_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, vnodeInfo* info)
{
    void* _buf = NULL;
    void*  buf = NULL;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(info);

    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8;
    len -= 8;

    ret = route->enc_ops->find_node_rsp(token, &route->ownId.id, info, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_FIND_NODE_RSP,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
    }
    return 0;
}

/*
 * @route:
 * @dest :
 * @hash :
 */
static
int _out_get_peers(struct vroute* route, vnodeAddr* dest, vnodeHash* hash)
{
    void* _buf = NULL;
    void*  buf = NULL;
    vtoken token;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(hash);

    vtoken_make(&token);
    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8;
    len -= 8;

    ret = route->enc_ops->get_peers(&token, &route->ownId.id, hash, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_GET_PEERS,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
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
int _out_get_peers_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, struct varray* peers)
{
    void* _buf = NULL;
    void*  buf = NULL;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(peers);

    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8;
    len -= 8;

    ret = route->enc_ops->get_peers_rsp(token, &route->ownId.id, peers, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_GET_PEERS_RSP,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
    }
    return 0;
}

/*
 * @route:
 * @dest:
 * @targetId:
 */
static
int _out_find_closest_nodes(struct vroute* route, vnodeAddr* dest, vnodeId* target)
{
    void* _buf = NULL;
    void*  buf = NULL;
    vtoken token;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(target);

    buf = _buf = vmem_aux_alloc(&route->mbuf_cache);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8;
    len -= 8;

    ret = route->enc_ops->find_closest_nodes(&token, &route->ownId.id, target, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_FIND_CLOSEST_NODES,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
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
int _out_find_closest_nodes_rsp(struct vroute* route, vnodeAddr* dest, vtoken* token, struct varray* closest)
{
    void* _buf = NULL;
    void*  buf = NULL;
    int len = BUF_SZ;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(closest);

    buf = _buf = vmem_aux_alloc(&route->mbuf_caches);
    vlog_cond((!buf), elog_vmem_aux_alloc);
    retE((!buf));

    buf += 8;
    len -= 8;

    ret = route->enc_ops->find_closest_nodes_rsp(token, &route->ownId.id, closest, buf, len);
    {
        struct vmsg_usr msg = {
            .addr  = &dest->addr,
            .msgId = VDHT_FIND_CLOSEST_NODES_RSP,
            .data  = _buf,
            .len   = ret + 8
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vmem_aux_free(&route->mbuf_cache, _buf));
    }
    return 0;
}

static
struct vroute_dht_ops route_dht_ops = {
    .ping                   = _out_ping,
    .ping_rsp               = _out_ping_rsp,
    .find_node              = _out_find_node,
    .find_node_rsp          = _out_find_node_rsp,
    .get_peers              = _out_get_peers,
    .get_peers_rsp          = _out_get_peers_rsp,
    .find_closest_nodes     = _out_find_closest_nodes,
    .find_closest_nodes_rsp = _out_find_closest_nodes_rsp
};

/*
 * @route:
 * @closest:
 */
static
int _in_ping(struct vroute* route, vtoken* token, vnodeAddr* from)
{
    vnodeInfo info;
    int ret = 0;

    vassert(route);
    vassert(token);
    vassert(from);

    ret = route->ops->add(route, from, PROP_PING);
    retE((ret < 0));

    vnodeInfo_init(&info, &route->ownId.id, &route->ownId.addr, route->flags, &route->version);
    ret = route->dht_ops->ping_reply(route, from, token, &info);
    retE((ret < 0));
    return 0;
}

static
int _in_ping_rsp(struct vroute* route, vnodeAddr* from, vnodeInfo* info)
{
    vnodeAddr node;
    uint32_t flags = 0;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(info);

    flags |= PROP_PING_RSP;
    vnodeAddr_init(&node, &info->id, &info->addr);
    if (vnodeVer_equal(&info->ver, &route->version)) {
        flags |= (info->flags | PROP_VER);
    }
    ret = route->ops->add(route, &node, flags);
    retE((ret < 0));
    return 0;
}

static
void _aux_vnodeInfo_free(void* info, void* cookie)
{
    vassert(info);
    vnodeInfo_free((vnodeInfo*)info);
    return ;
}

static
int _in_find_node(struct vroute* route, vtoken* token, vnodeAddr* from, vnodeId* targetId)
{
    struct varray closest;
    vnodeInfo info;
    int ret = 0;

    vassert(route);
    vassert(token);
    vassert(from);
    vassert(targetId);

    ret = route->ops->add(route, from, PROP_FIND_NODE);
    retE((ret < 0));

    ret = route->ops->find(route, targetId, &info);
    if (!ret) { // if found, then sending reply.
        ret = route->dht_ops->find_node_rsp(route, from, token, &info);
        retE((ret < 0));
        return 0;
    }

    // otherwise, have to sending find_closest_nodes reply.
    varray_init(&closest, MAX_CAPC);
    ret = route->ops->find_closest_nodes(route, targetId, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));

    ret = route->dht_ops->find_closest_nodes_rsp(route, from, token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));

    return 0;
}

static
int _in_find_node_rsp(struct vroute* route, vnodeAddr* from, vnodeInfo* info)
{
    vnodeAddr node;
    uint32_t flags;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(info);

    ret = route->ops->add(route, from, PROP_FIND_NODE_RSP);
    retE((ret < 0));

    vnodeAddr_init(&node, &info->id, &info->addr);
    if (vnodeVer_equal(&info->ver, &route->version)) {
        flags = info->flags | PROP_VER;
    }
    ret = route->ops->add(route, &node, flags);
    retE((ret < 0));
    return 0;
}

static
int _in_get_peers(struct vroute* route, vtoken* token, vnodeAddr* from, vnodeHash* hash)
{
    struct varray peers;
    int ret = 0;

    vassert(route);
    vassert(token);
    vassert(from);
    vassert(hash);

    ret = route->ops->add(route, from, PROP_GET_PEERS);
    retE((ret < 0));

    varray_init(&peers, MAX_CAPC);
    ret = route->ops->get_peers(route, hash, &peers, MAX_CAPC);
    if (!ret) {
        ret = route->dht_ops->get_peers_rsp(route, from, token, &peers);
        varray_zero(&peers, _aux_vnodeInfo_free, NULL);
        varray_deinit(&peers);
        retE((ret < 0));
        return 0;
    }
    ret = route->ops->find_closest_nodes(route, &from->id, &peers, MAX_CAPC);
    if (!ret) {
        ret = route->dht_ops->find_closest_nodes_rsp(route, from, token, &peers);
        varray_zero(&peers, _aux_vnodeInfo_free, NULL);
        varray_deinit(&peers);
        retE((ret < 0));
        return 0;
    }
    varray_deinit(&peers);
    retE((ret < 0));
    return 0;
}

static
int _in_get_peers_rsp(struct vroute* route, vnodeAddr* from, struct varray* array)
{
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(array);

    ret = route->ops->add(route, from, PROP_GET_PEERS_RSP);
    retE((ret < 0));

    //TODO;
    return 0;
}

static
int _in_find_closest_nodes(struct vroute* route, vtoken* token, vnodeAddr* from, vnodeId* target)
{
    struct varray closest;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(target);

    ret = route->ops->add(route, from, PROP_FIND_CLOSEST_NODES);
    retE((ret < 0));

    varray_init(&closest, MAX_CAPC);
    ret = route->ops->find_closest_nodes(route, target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));

    ret = route->dht_ops->find_closest_nodes_rsp(route, from, token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

static
int _in_find_closest_nodes_rsp(struct vroute* route, vnodeAddr* from, struct varray* closest)
{
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(from);
    vassert(closest);

    ret = route->ops->add(route, from, PROP_FIND_CLOSEST_NODES_RSP);
    retE((ret < 0));

    for (; i < varray_size(closest); i++) {
        vnodeInfo* info = NULL;
        vnodeAddr  node;
        uint32_t  flags;

        info = (vnodeInfo*)varray_get(closest, i);
        vnodeAddr_init(&node, &info->id, &info->addr);
        if (vnodeVer_equal(&info->ver, &route->version)) {
            flags = info->flags | PROP_VER;
        }
        route->ops->add(route, &node, flags);
    }
    return 0;
}

static
struct vroute_cb_ops route_cb_ops = {
    .ping                   = _in_ping,
    .ping_rsp               = _in_ping_rsp,
    .find_node              = _in_find_node,
    .find_node_rsp          = _in_find_node_rsp,
    .get_peers              = _in_get_peers,
    .get_peers_rsp          = _in_get_peers_rsp,
    .find_closest_nodes     = _in_find_closest_nodes,
    .find_closest_nodes_rsp = _in_find_closest_nodes_rsp
};

static
int _vroute_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vroute* route = (struct vroute*)cookie;
    struct vdht_dec_ops* dec_ops = NULL;
    vnodeAddr addr;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(mu);

    dec_ops = route->dec_ops;
    vsockaddr_copy(&addr.addr, mu->addr);

    switch(mu->msgId) {
    case VDHT_PING: {
        ret = dec_ops->ping(mu->data, mu->len, &token, &addr.id);
        retE((ret < 0));
        ret = route->cb_ops->ping(route, &token, &addr);
        retE((ret < 0));
        break;
    }
    case VDHT_PING_RSP: {
        vnodeInfo info;
        ret = dec_ops->ping_rsp(mu->data, mu->len, &token, &addr.id, &info);
        retE((ret < 0));
        ret = route->cb_ops->ping_rsp(route, &addr, &info);
        retE((ret < 0));
        break;
    }
    case VDHT_FIND_NODE: {
        vnodeId targetId;
        ret = dec_ops->find_node(mu->data, mu->len, &token, &addr.id, &targetId);
        retE((ret < 0));
        vsockaddr_copy(&addr.addr, mu->addr);
        ret = route->cb_ops->find_node(route, &token, &addr, &targetId);
        retE((ret < 0));
        break;
    }
    case VDHT_FIND_NODE_RSP: {
        vnodeInfo info;
        ret = dec_ops->find_node_rsp(mu->data, mu->len, &token, &addr.id, &info);
        retE((ret < 0));
        ret = route->cb_ops->find_node_rsp(route, &addr, &info);
        retE((ret < 0));
        break;
    }

    case VDHT_GET_PEERS: {
        vnodeHash hash;
        ret = dec_ops->get_peers(mu->data, mu->len, &token, &addr.id, &hash);
        retE((ret < 0));
        ret = route->cb_ops->get_peers(route, &token, &addr, &hash);
        retE((ret < 0));
        break;
    }
    case VDHT_GET_PEERS_RSP: {
        struct varray peers;
        varray_init(&peers, MAX_CAPC);
        ret = dec_ops->get_peers_rsp(mu->data, mu->len, &token, &addr.id, &peers);
        retE((ret < 0));
        ret = route->cb_ops->get_peers_rsp(route, &addr, &peers);
        varray_zero(&peers, _aux_vnodeInfo_free, NULL);
        varray_deinit(&peers);
        retE((ret < 0));
        break;
    }
    case VDHT_FIND_CLOSEST_NODES: {
        vnodeId targetId;
        ret = dec_ops->find_closest_nodes(mu->data, mu->len, &token, &addr.id, &targetId);
        retE((ret < 0));
        ret = route->cb_ops->find_closest_nodes(route, &token, &addr, &targetId);
        retE((ret < 0));
        break;
    }
    case VDHT_FIND_CLOSEST_NODES_RSP: {
        struct varray closest;
        varray_init(&closest, MAX_CAPC);
        ret = dec_ops->find_closest_nodes_rsp(mu->data, mu->len, &token, &addr.id, &closest);
        retE((ret < 0));
        ret = route->cb_ops->find_closest_nodes_rsp(route, &addr, &closest);
        varray_zero(&closest, _aux_vnodeInfo_free, NULL);
        varray_deinit(&closest);
        retE((ret < 0));
        break;
    }
    default:
        break;
    }
    return 0;
}


int vroute_init(struct vroute* route, struct vmsger* msger, vnodeAddr* addr)
{
    int i = 0;
    vassert(route);
    vassert(msger);
    vassert(addr);

    vnodeAddr_copy(&route->ownId, addr);
    route->msger   = msger;
    route->ops     = &route_ops;
    route->dht_ops = &route_dht_ops;
    route->cb_ops  = &route_cb_ops;
    route->enc_ops = &dht_enc_ops;
    route->dec_ops = &dht_dec_ops;

    vlock_init(&route->lock);
    for (; i < NBUCKETS; i++) {
        varray_init(&route->bucket[i].peers, 0);
        route->bucket[i].ts = 0;
    }
    vmem_aux_init(&route->mbuf_cache, BUF_SZ, 8);

    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_PING );
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_PING_RSP);
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_FIND_NODE);
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_FIND_NODE_RSP);
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_GET_PEERS);
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_GET_PEERS_RSP);
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_FIND_CLOSEST_NODES);
    msger->ops->add_cb(route->msger, route, _vroute_msg_cb, VDHT_FIND_CLOSEST_NODES_RSP);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    int i = 0;
    vassert(route);

    vmem_aux_deinit(&route->mbuf_cache);
    route->ops->clear(route);
    for (; i < NBUCKETS; i++) {
        varray_deinit(&route->bucket[i].peers);
    }
    vlock_deinit(&route->lock);
    return ;
}

