#include "vglobal.h"
#include "vroute.h"

#define MAX_CAPC ((int)8)

/*
 * the routine to join a node with well known address into routing table,
 * whose ID usually is fake and trivial.
 *
 * @route: routing table.
 * @node:  well-known address of node to be joined
 *
 */
static
int _vroute_join_node(struct vroute* route, struct sockaddr_in* addr)
{
    struct vroute_node_space* node_space = &route->node_space;
    vnodeInfo_relax nodei_relax;
    vnodeInfo* nodei = (vnodeInfo*)&nodei_relax;
    vtoken id;
    int ret = 0;

    vassert(route);
    vassert(addr);

    vtoken_make(&id);
    vnodeInfo_relax_init(&nodei_relax, &id, vnodeVer_unknown(), 0);
    vnodeInfo_add_addr(&nodei, addr);

    vlock_enter(&route->lock);
    ret = node_space->ops->add_node(node_space, nodei, 0);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}


/*
 * the routine to find best suitable service from service routing table
 * so that local app can meet its needs with that service.
 *
 * @route:
 * @svc_hash: service hash id.
 * @addr : [out] address of service
 */
static
int _vroute_find_service(struct vroute* route, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vsrvcInfo_relax srvci;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    memset(&srvci, 0, sizeof(srvci));
    srvci.capc = VSRVCINFO_MAX_ADDRS;

    vlock_enter(&route->lock);
    ret = srvc_space->ops->get_service(srvc_space, hash, (vsrvcInfo*)&srvci);
    vlock_leave(&route->lock);
    retE((ret < 0));

    if (!ret) {
        ncb(&srvci.hash, 0, VPROTO_UNKNOWN, cookie);
        return 0;
    }

    ncb(&srvci.hash, srvci.naddrs, vsrvcInfo_proto((vsrvcInfo*)&srvci), cookie);
    for (i = 0; i < srvci.naddrs; i++) {
        icb(&srvci.hash, &srvci.addrs[i], ((i+1) == srvci.naddrs), cookie);
    }
    return 0;
}

static
int _vroute_probe_service(struct vroute* route, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vroute_srvc_probe_helper* probe_helper = &route->probe_helper;
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;

    vassert(route);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    vlock_enter(&route->lock);
    ret = node_space->ops->probe_service(node_space, hash);
    if (ret < 0) {
        vlock_leave(&route->lock);
        retE((1));
    }
    probe_helper->ops->add(probe_helper, hash, ncb, icb, cookie);
    vlock_leave(&route->lock);
    return 0;
}

static
int _vroute_air_service(struct vroute* route, vsrvcInfo* srvci)
{
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;
    vassert(route);
    vassert(srvci);

    vlock_enter(&route->lock);
    ret = node_space->ops->air_service(node_space, srvci);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

static
int _vroute_reflex_addr(struct vroute* route, struct sockaddr_in* addr)
{
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;
    vassert(route);
    vassert(addr);

    vlock_enter(&route->lock);
    ret = node_space->ops->reflex_addr(node_space, addr);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

int _vroute_probe_connectivity(struct vroute* route, struct sockaddr_in* laddr)
{
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;

    vassert(route);
    vassert(laddr);

    vlock_enter(&route->lock);
    ret = node_space->ops->probe_connectivity(node_space, laddr);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

void _vroute_set_inspect_cb(struct vroute* route, vroute_inspect_t cb, void* cookie)
{
    vassert(route);
    vassert(cb);

    vlock_enter(&route->lock);
    route->insp_cb = cb;
    route->insp_cookie = cookie;
    vlock_leave(&route->lock);

    return ;
}

void _vroute_inspect(struct vroute* route, vtoken* token, uint32_t insp_id)
{
    vassert(route);
    vassert(token);

    vlock_enter(&route->lock);
    if (route->insp_cb) {
        route->insp_cb(route, route->insp_cookie, token, insp_id);
    }
    vlock_leave(&route->lock);
    return ;
}

/*
 * the routine to load routing table infos from route database when host
 * starts at first.
 *
 * @route:
 */
static
int _vroute_load(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;
    vassert(route);

    vlock_enter(&route->lock);
    ret = node_space->ops->load(node_space);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to write all nodes in node routing table back to route database
 * as long as the host become offline
 *
 * @route:
 */
static
int _vroute_store(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;
    vassert(route);

    vlock_enter(&route->lock);
    ret = node_space->ops->store(node_space);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to priodically refresh the routing table.
 *
 * @route:
 */
static
int _vroute_tick(struct vroute* route)
{
    struct vroute_srvc_probe_helper* probe_helper = &route->probe_helper;
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->tick(node_space);
    recr_space->ops->timed_reap(recr_space);// reap all timeout records.
    probe_helper->ops->timed_reap(probe_helper);
    vlock_leave(&route->lock);
    return 0;
}

/*
 * the routine to clean routing table
 *
 * @route:
 */
static
void _vroute_clear(struct vroute* route)
{
    struct vroute_srvc_probe_helper* probe_helper = &route->probe_helper;
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->clear(node_space);
    srvc_space->ops->clear(srvc_space);
    recr_space->ops->clear(recr_space);
    probe_helper->ops->clear(probe_helper);
    vlock_leave(&route->lock);

    return;
}

/* the routine to dump routing table
 *
 * @route:
 */
static
void _vroute_dump(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vassert(route);

    vdump(printf("-> ROUTE"));
    vlock_enter(&route->lock);
    node_space->ops->dump(node_space);
    srvc_space->ops->dump(srvc_space);
    vlock_leave(&route->lock);
    vdump(printf("<- ROUTE"));
    return;
}

static
struct vroute_ops route_ops = {
    .join_node     = _vroute_join_node,
    .find_service  = _vroute_find_service,
    .probe_service = _vroute_probe_service,
    .air_service   = _vroute_air_service,
    .reflex        = _vroute_reflex_addr,
    .probe_connectivity = _vroute_probe_connectivity,
    .set_inspect_cb     = _vroute_set_inspect_cb,
    .inspect       = _vroute_inspect,
    .load          = _vroute_load,
    .store         = _vroute_store,
    .tick          = _vroute_tick,
    .clear         = _vroute_clear,
    .dump          = _vroute_dump
};



/*
 * the routine to pack a @ping query and send it.
 * @route:
 * @conn:
 */
static
int _vroute_dht_ping(struct vroute* route, vnodeConn* conn)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    retS((!(route->props & PROP_PING))); //ping disabled.

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->ping(&token, &route->myid, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->ops->inspect(route, &token, VROUTE_INSP_SND_PING);

    // make a record according to the query, which will be used to
    // check invality of response msg.
    recr_space->ops->make(recr_space, &token);
    vlogD("send @ping");
    return 0;
}


/*
 * the routine to pack and send a response to @ping query back to source node
 * where the ping query was from.
 * @route:
 * @conn:
 * @token:
 * @info :
 */
static
int _vroute_dht_ping_rsp(struct vroute* route, vnodeConn* conn, vtoken* token, vnodeInfo* nodei)
{
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(token);
    vassert(nodei);
    retS((!(route->props & PROP_PING_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = route->enc_ops->ping_rsp(token, &route->myid, nodei, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @ping_rsp");
    return 0;
}

/*
 * the routine to pack and send @find_node query to specific node to get infos
 * of a given node.
 * @route:
 * @conn:
 * @target
 */
static
int _vroute_dht_find_node(struct vroute* route, vnodeConn* conn, vnodeId* targetId)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(targetId);
    retS((!(route->props & PROP_FIND_NODE)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->find_node(&token, &route->myid, targetId, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->ops->inspect(route, &token, VROUTE_INSP_SND_FIND_NODE);
    recr_space->ops->make(recr_space, &token);
    vlogD("send @find_node");
    return 0;
}

/*
 * the routine to pack and send a response to @find_node query.
 * @route:
 * @conn:
 * @token:
 * @info:
 */
static
int _vroute_dht_find_node_rsp(struct vroute* route, vnodeConn* conn, vtoken* token, vnodeInfo* nodei)
{
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(token);
    vassert(nodei);
    retS((!(route->props & PROP_FIND_NODE_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = route->enc_ops->find_node_rsp(token, &route->myid, nodei, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @find_node_rsp");
    return 0;
}

/*
 * the routine to pack and send a @find_closest_nodes query( closest to @targetId).
 * @route:
 * @conn:
 * @targetId:
 */
static
int _vroute_dht_find_closest_nodes(struct vroute* route, vnodeConn* conn, vnodeId* targetId)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(targetId);
    retS((!(route->props & PROP_FIND_CLOSEST_NODES)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->find_closest_nodes(&token, &route->myid, targetId, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->ops->inspect(route, &token, VROUTE_INSP_SND_FIND_CLOSEST_NODES);
    recr_space->ops->make(recr_space, &token);
    vlogD("send @find_closest_nodes");
    return 0;
}

/*
 * the routine to pack and send back a response to @find_closest_nodes query.
 * @route:
 * @conn:
 * @token:
 * @closest: array of closest nodes (closest to given id);
 */
static
int _vroute_dht_find_closest_nodes_rsp(struct vroute* route, vnodeConn* conn, vtoken* token, struct varray* closest)
{
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(token);
    vassert(closest);
    retS((!(route->props & PROP_FIND_CLOSEST_NODES_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = route->enc_ops->find_closest_nodes_rsp(token, &route->myid, closest, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @find_closest_nodes_rsp");
    return 0;
}

/*
 * the routine to pack and send a @reflex_addr query.
 * @route:
 * @conn:
 */
static
int _vroute_dht_reflex(struct vroute* route, vnodeConn* conn)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->reflex(&token, &route->myid, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->ops->inspect(route, &token, VROUTE_INSP_SND_REFLEX);
    recr_space->ops->make(recr_space, &token);
    vlogD("send @reflex");
    return 0;
}

/*
 * the routine to pack and send back a response to @reflex_addr query.
 * @route:
 * @conn:
 * @token:
 * @reflexive_addr:
 */
static
int _vroute_dht_reflex_rsp(struct vroute* route, vnodeConn* conn, vtoken* token, struct sockaddr_in* reflexive_addr)
{
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(token);
    vassert(reflexive_addr);

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = route->enc_ops->reflex_rsp(token, &route->myid, reflexive_addr, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @reflex_rsp");
    return 0;
}

/*
 * the routine to pack and send a @probe_connectivity query
 * @route:
 * @conn:
 * @targetId:
 */
static
int _vroute_dht_probe(struct vroute* route, vnodeConn* conn, vnodeId* targetId)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(targetId);

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->probe(&token, &route->myid, targetId, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->ops->inspect(route, &token, VROUTE_INSP_SND_PROBE);
    recr_space->ops->make(recr_space, &token);
    vlogD("send @probe");
    return 0;
}

/*
 * the routine to pack and send back a response to @probe_connectivity query.
 * @route:
 * @conn:
 * @token:
 */
static
int _vroute_dht_probe_rsp(struct vroute* route, vnodeConn* conn, vtoken* token)
{
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(token);

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = route->enc_ops->probe_rsp(token, &route->myid, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @probe_rsp");
    return 0;
}

/*
 * the routine to pack and send @post_service indication.
 * @route:
 * @conn:
 * @srvc:
 */
static
int _vroute_dht_post_service(struct vroute* route, vnodeConn* conn, vsrvcInfo* srvci)
{
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(srvci);
    retS((!(route->props & PROP_POST_SERVICE)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->post_service(&token, &route->myid, srvci, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @post_service");
    return 0;
}

/*
 * the routine to pack and send a @find_service query.
 * @route:
 * @conn:
 * @srvcId:
 */
static
int _vroute_dht_find_service(struct vroute* route, vnodeConn* conn, vsrvcHash* hash)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(hash);
    retS((!(route->props & PROP_FIND_SERVICE)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = route->enc_ops->find_service(&token, &route->myid, hash, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }

    route->ops->inspect(route, &token, VROUTE_INSP_SND_FIND_SERVICE);
    recr_space->ops->make(recr_space, &token);
    vlogD("send @find_service");
    return 0;
}

/*
 * the routine to pack and send a response to @find_service query.
 * @route:
 * @conn:
 * @token:
 * @srvcs:
 */
static
int _vroute_dht_find_service_rsp(struct vroute* route, vnodeConn* conn, vtoken* token, vsrvcInfo* srvc)
{
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(token);
    vassert(srvc);
    retS((!(route->props & PROP_FIND_SERVICE_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = route->enc_ops->find_service_rsp(token, &route->myid, srvc, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(&conn->remote),
            .spec  = to_vsockaddr_from_sin(&conn->local),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    vlogD("send @find_service_rsp");
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
    .reflex                 = _vroute_dht_reflex,
    .reflex_rsp             = _vroute_dht_reflex_rsp,
    .probe                  = _vroute_dht_probe,
    .probe_rsp              = _vroute_dht_probe_rsp,
    .post_service           = _vroute_dht_post_service,
    .find_service           = _vroute_dht_find_service,
    .find_service_rsp       = _vroute_dht_find_service_rsp
};

static
void _aux_vnodeInfo_free(void* item, void* cookie)
{
    vnodeInfo_relax* nodei = (vnodeInfo_relax*)item;
    vassert(nodei);

    vnodeInfo_relax_free(nodei);
    return ;
}

/* the routine to call when receiving a @ping query.
 *
 * @route:
 * @conn:
 * @ctxt:  dht decoder context.
 */
static
int _vroute_cb_ping(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    vnodeInfo_relax nodei_relax;
    vnodeInfo* nodei = (vnodeInfo*)&nodei_relax;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret =route->dec_ops->ping(ctxt, &token, &fromId);
    retE((ret < 0));

    vnodeInfo_relax_init(&nodei_relax, &fromId, vnodeVer_unknown(), 0);
    vnodeInfo_add_addr(&nodei, &conn->remote);
    ret = node_space->ops->add_node(node_space, nodei, 1);
    retE((ret < 0));

    route->ops->inspect(route, &token, VROUTE_INSP_RCV_PING);
    ret = route->dht_ops->ping_rsp(route, conn, &token, (vnodeInfo*)&route->node->nodei);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a response to @ping query.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_ping_rsp(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_recr_space* recr_space = &route->recr_space;
    vnodeInfo_relax nodei;
    vtoken  token;
    int ret = 0;

    ret = route->dec_ops->ping_rsp(ctxt, &token, (vnodeInfo*)&nodei);
    retE((ret < 0));
    retE((!recr_space->ops->check(recr_space, &token))); // skip vicious response.

    ret = node_space->ops->add_node(node_space, (vnodeInfo*)&nodei, 1);
    retE((ret < 0));
    route->ops->inspect(route, &token, VROUTE_INSP_RCV_PING_RSP);
    return 0;
}

/*
 * the routine to call when receiving a @find_node query.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_find_node(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    vnodeInfo_relax nodei;
    struct varray closest;
    vnodeId targetId;
    vnodeId fromId;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_node(ctxt, &token, &fromId, &targetId);
    retE((ret < 0));
    ret = node_space->ops->get_node(node_space, &targetId, (vnodeInfo*)&nodei);
    retE((ret < 0));
    if (ret == 1) { //means found
        ret = route->dht_ops->find_node_rsp(route, conn, &token, (vnodeInfo*)&nodei);
        retE((ret < 0));
        return 0;
    }

    // otherwise, send @find_closest_nodes_rsp response instead.
    varray_init(&closest, MAX_CAPC);
    ret = node_space->ops->get_neighbors(node_space, &targetId, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    if (ret == 0) { //means no closest nodes.
        varray_deinit(&closest);
        return 0;
    }
    ret = route->dht_ops->find_closest_nodes_rsp(route, conn, &token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receiving a response to @find_node query.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_find_node_rsp(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_recr_space* recr_space = &route->recr_space;
    vnodeInfo_relax nodei;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_node_rsp(ctxt, &token, &fromId, (vnodeInfo*)&nodei);
    retE((ret < 0));
    retE((!recr_space->ops->check(recr_space, &token)));

    ret = node_space->ops->add_node(node_space, (vnodeInfo*)&nodei, 0);
    retE((ret < 0));

    route->ops->inspect(route, &token, VROUTE_INSP_RCV_FIND_NODE_RSP);
    return 0;
}

/*
 * the routine to call when receiving a @find_closest_nodes query.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_find_closest_nodes(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct varray closest;
    vnodeId targetId;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_closest_nodes(ctxt, &token, &fromId, &targetId);
    retE((ret < 0));

    varray_init(&closest, MAX_CAPC);
    ret = node_space->ops->get_neighbors(node_space, &targetId, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    if (ret == 0) {
        varray_deinit(&closest);
        return 0; // not response if no closest nodes found.
    }

    ret = route->dht_ops->find_closest_nodes_rsp(route, conn, &token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receiving a response to @find_closest_nodes
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_find_closest_nodes_rsp(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    struct varray closest;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    varray_init(&closest, MAX_CAPC);
    ret = route->dec_ops->find_closest_nodes_rsp(ctxt, &token, &fromId, &closest);
    ret1E((ret < 0), varray_deinit(&closest));
    if (!recr_space->ops->check(recr_space, &token)) {
        varray_zero(&closest, _aux_vnodeInfo_free, NULL);
        varray_deinit(&closest);
        return -1;
    }

    for (i = 0; i < varray_size(&closest); i++) {
        node_space->ops->add_node(node_space, (vnodeInfo*)varray_get(&closest, i), 0);
    }
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);

    route->ops->inspect(route, &token, VROUTE_INSP_RCV_FIND_CLOSEST_NODES_RSP);
    return 0;
}

/*
 * the routine to call when receiving a @reflex query.
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_reflex(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->reflex(ctxt, &token, &fromId);
    retE((ret < 0));

    ret = route->dht_ops->reflex_rsp(route, conn, &token, &conn->remote);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receiving a response to @reflex
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_reflex_rsp(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vnode* node = route->node;
    struct sockaddr_in reflexive_addr;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->reflex_rsp(ctxt, &token, &fromId, &reflexive_addr);
    retE((ret < 0));
    retE((!recr_space->ops->check(recr_space, &token)));

    ret = node->ops->reflex_addr(node, &conn->local, &reflexive_addr);
    retE((ret < 0));
    route->ops->inspect(route, &token, VROUTE_INSP_RCV_REFLEX_RSP);
    return 0;
}

/*
 * the routine to call when receiving a @probe query.
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_probe(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    vnodeId targetId;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->probe(ctxt, &token, &fromId, &targetId);
    retE((ret < 0));
    retE((!vtoken_equal(&targetId, &route->myid)));

    ret = route->dht_ops->probe_rsp(route, conn, &token);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receiving a response to @probe query.
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_probe_rsp(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->probe_rsp(ctxt, &token, &fromId);
    retE((ret < 0));
    retE((!recr_space->ops->check(recr_space, &token)));

    ret = node_space->ops->adjust_connectivity(node_space, &fromId, conn);
    retE((ret < 0));
    route->ops->inspect(route, &token, VROUTE_INSP_RCV_PROBE_RSP);
    return 0;
}

/*
 * the routine to call when receiving a @post_service indication.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_post_service(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vsrvcInfo_relax srvci;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->post_service(ctxt, &token, &fromId, (vsrvcInfo*)&srvci);
    retE((ret < 0));
    ret = srvc_space->ops->add_service(srvc_space, (vsrvcInfo*)&srvci);
    retE((ret < 0));
    route->ops->inspect(route, &token, VROUTE_INSP_RCV_POST_SERVICE);
    return 0;
}

/*
 * the routine to call when receiving a @find_service query.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_find_service(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vsrvcInfo_relax srvci;
    vsrvcHash srvcHash;
    vnodeId fromId;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    memset(&srvci, 0, sizeof(srvci));
    srvci.capc = VSRVCINFO_MAX_ADDRS;

    ret = route->dec_ops->find_service(ctxt, &token, &fromId, &srvcHash);
    retE((ret < 0));
    ret = srvc_space->ops->get_service(srvc_space, &srvcHash, (vsrvcInfo*)&srvci);
    retE((ret < 0));
    retS((ret == 0));
    ret = route->dht_ops->find_service_rsp(route, conn, &token, (vsrvcInfo*)&srvci);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receiving a response to @find_service.
 *
 * @route:
 * @conn:
 * @ctxt:
 */
static
int _vroute_cb_find_service_rsp(struct vroute* route, vnodeConn* conn, void* ctxt)
{
    struct vroute_srvc_probe_helper* probe_helper = &route->probe_helper;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    vsrvcInfo_relax srvci;
    vnodeId fromId;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(ctxt);
    vassert(conn);

    ret = route->dec_ops->find_service_rsp(ctxt, &token, &fromId, (vsrvcInfo*)&srvci);
    retE((ret < 0));
    retE((!recr_space->ops->check(recr_space, &token)));

    ret = srvc_space->ops->add_service(srvc_space, (vsrvcInfo*)&srvci);
    retE((ret < 0));

    //try to add info of node hosting that service.
    ret = node_space->ops->probe_node(node_space, &srvci.hostid);
    retE((ret < 0));
    route->ops->inspect(route, &token, VROUTE_INSP_RCV_FIND_SERVICE_RSP);
    probe_helper->ops->invoke(probe_helper, &srvci.hash, (vsrvcInfo*)&srvci);
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
    _vroute_cb_reflex,
    _vroute_cb_reflex_rsp,
    _vroute_cb_probe,
    _vroute_cb_probe_rsp,
    _vroute_cb_post_service,
    _vroute_cb_find_service,
    _vroute_cb_find_service_rsp,
    NULL
};

static
int _aux_route_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vroute* route = (struct vroute*)cookie;
    vnodeConn conn;
    void* ctxt = NULL;
    int   ret  = 0;

    vassert(route);
    vassert(mu);

    ret = route->dec_ops->dec_begin(mu->data, mu->len, &ctxt);
    retE((ret >= VDHT_UNKNOWN));
    retE((ret < 0));
    vlogD("received @%s", vdht_get_desc(ret));

    vnodeConn_set(&conn, to_sockaddr_sin(mu->spec), to_sockaddr_sin(mu->addr));
    vlock_enter(&route->lock);
    ret = route->cb_ops[ret](route, &conn, ctxt);
    vlock_leave(&route->lock);
    route->dec_ops->dec_done(ctxt);
    retE((ret < 0));
    return 0;
}

static
int _aux_route_load_proto_caps(struct vconfig* cfg, uint32_t* props)
{
    struct varray* tuple = NULL;
    int i = 0;

    vassert(cfg);
    vassert(props);

    tuple = cfg->ops->get_tuple_val(cfg, "dht.protocol");
    if (!tuple) {
        *props |= PROP_PING;
        *props |= PROP_PING_R;
        return 0;
    }
    for (i = 0; i < varray_size(tuple); i++) {
        struct vcfg_item* item = NULL;
        int dht_id = -1;

        item = (struct vcfg_item*)varray_get(tuple, i);
        retE((item->type != CFG_STR));
        dht_id = vdht_get_dhtId_by_desc(item->val.s);
        retE((dht_id < 0));
        retE((dht_id >= VDHT_UNKNOWN));

        *props |= (1 << dht_id);
    }
    return 0;
}

int vroute_init(struct vroute* route, struct vconfig* cfg, struct vhost* host, vnodeId* myid)
{
    vassert(route);
    vassert(host);
    vassert(myid);

    vtoken_copy(&route->myid, myid);
    _aux_route_load_proto_caps(cfg, &route->props);

    vlock_init(&route->lock);
    vroute_node_space_init(&route->node_space, route, cfg, myid);
    vroute_srvc_space_init(&route->srvc_space, cfg);
    vroute_recr_space_init(&route->recr_space);
    vroute_srvc_probe_helper_init(&route->probe_helper);

    route->ops     = &route_ops;
    route->dht_ops = &route_dht_ops;
    route->cb_ops  = route_cb_ops;
    route->enc_ops = &dht_enc_ops;
    route->dec_ops = &dht_dec_ops;

    route->cfg   = cfg;
    route->msger = &host->msger;
    route->node  = &host->node;

    route->insp_cb = NULL;
    route->insp_cookie = NULL;

    route->msger->ops->add_cb(route->msger, route, _aux_route_msg_cb, VMSG_DHT);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    vassert(route);

    vroute_srvc_probe_helper_deinit (&route->probe_helper);
    vroute_recr_space_deinit(&route->recr_space);
    vroute_node_space_deinit(&route->node_space);
    vroute_srvc_space_deinit(&route->srvc_space);
    vlock_deinit(&route->lock);
    return ;
}

