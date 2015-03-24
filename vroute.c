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
    vnodeInfo_relax nodei;
    int ret = 0;

    vassert(route);
    vassert(addr);

    {
        vtoken id;
        vtoken_make(&id);
        vnodeInfo_relax_init(&nodei, &id, &unknown_node_ver, 0);
        vnodeInfo_add_addr((vnodeInfo*)&nodei, addr);
    }
    vlock_enter(&route->lock);
    ret = node_space->ops->add_node(node_space, (vnodeInfo*)&nodei, 0);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to probe best suitable service node from service routing table
 * so that local app can meet its needs with that service.
 *
 * @route:
 * @svc_hash: service hash id.
 * @addr : [out] address of service
 */
static
int _vroute_probe_service(struct vroute* route, vsrvcId* srvcId, vsrvcInfo_iterate_addr_t cb, void* cookie)
{
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vsrvcInfo* srvci = NULL;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(srvcId);
    vassert(cb);

    vlock_enter(&route->lock);
    ret = srvc_space->ops->get_service(srvc_space, srvcId, &srvci);
    vlock_leave(&route->lock);
    retE((ret < 0));
    retE((ret == 0) && (!srvci));

    for (i = 0; i < srvci->naddrs; i++) {
        cb(&srvci->addr[i], cookie);
    }
    vsrvcInfo_free(srvci);
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
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->tick(node_space);
    vlock_leave(&route->lock);
    recr_space->ops->timed_reap(recr_space);// reap all timeout records.
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
    struct vroute_recr_space* recr_space = &route->recr_space;
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->clear(node_space);
    srvc_space->ops->clear(srvc_space);
    vlock_leave(&route->lock);
    recr_space->ops->clear(recr_space);

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
    .probe_service = _vroute_probe_service,
    .air_service   = _vroute_air_service,
    .reflex        = _vroute_reflex_addr,
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

    // make a record according to the query, which will be used to
    // check invality of corresponding response.
    recr_space->ops->make(recr_space, &token);
    vlogI(printf("send @ping"));
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
    vlogI(printf("send @ping_rsp"));
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
    recr_space->ops->make(recr_space, &token);
    vlogI(printf("send @find_node"));
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
    vlogI(printf("send @find_node_rsp"));
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
    recr_space->ops->make(recr_space, &token);
    vlogI(printf("send @find_closest_nodes"));
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
    vlogI(printf("send @find_closest_nodes_rsp"));
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
    recr_space->ops->make(recr_space, &token);
    vlogI(printf("send @reflex"));
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
    vlogI(printf("send @reflex_rsp"));
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
    vlogI(printf("send @post_service"));
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
    .post_service           = _vroute_dht_post_service
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
    vnodeInfo_relax from;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret =route->dec_ops->ping(ctxt, &token, &fromId);
    retE((ret < 0));

    vnodeInfo_relax_init(&from, &fromId, &unknown_node_ver, 0);
    vnodeInfo_add_addr((vnodeInfo*)&from, &conn->remote);
    ret = node_space->ops->add_node(node_space, (vnodeInfo*)&from, 1);
    retE((ret < 0));

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

    ret = route->dec_ops->reflex_rsp(route, &token, &fromId, &reflexive_addr);
    retE((ret < 0));
    retE((!recr_space->ops->check(recr_space, &token)));

    ret = node->ops->reflex_addr(node, &conn->local, &reflexive_addr);
    retE((ret < 0));
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
    vsrvcInfo* srvc = NULL;
    vnodeId fromId;
    vtoken  token;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->post_service(ctxt, &token, &fromId, &srvc);
    retE((ret < 0));
    ret = srvc_space->ops->add_service(srvc_space, srvc);
    vsrvcInfo_free(srvc);
    retE((ret < 0));
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
    _vroute_cb_post_service,
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
    vlogI(printf("received @%s", vdht_get_desc(ret)));

    vnodeConn_set(&conn, to_sockaddr_sin(mu->spec), to_sockaddr_sin(mu->addr));
    ret = route->cb_ops[ret](route, &conn, ctxt);
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
        int   dht_id  = -1;

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

    route->ops     = &route_ops;
    route->dht_ops = &route_dht_ops;
    route->cb_ops  = route_cb_ops;
    route->enc_ops = &dht_enc_ops;
    route->dec_ops = &dht_dec_ops;

    route->cfg   = cfg;
    route->msger = &host->msger;
    route->node  = &host->node;

    route->msger->ops->add_cb(route->msger, route, _aux_route_msg_cb, VMSG_DHT);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    vassert(route);

    vroute_recr_space_deinit(&route->recr_space);
    vroute_node_space_deinit(&route->node_space);
    vroute_srvc_space_deinit(&route->srvc_space);
    vlock_deinit(&route->lock);
    return ;
}

