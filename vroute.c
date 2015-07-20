#include "vglobal.h"
#include "vroute.h"

#define MAX_CAPC ((int)8)

/*
 * the routine to join a node with well known address into routing table,
 * whose ID usually is fake and trivial.
 *
 * @route: routing table space
 * @node:  well-known address of node to join.
 *
 */
static
int _vroute_join_node(struct vroute* route, struct sockaddr_in* addr)
{
    struct vroute_node_space* node_space = &route->node_space;
    DECL_VNODE_RELAX(nodei);
    vnodeId id;
    int ret = 0;

    vassert(route);
    vassert(addr);

    /*
     * make up a fake DHT nodei, which will lead to interconnect with
     * real DHT node with same address.
     */
    vtoken_make(&id);
    vnodeInfo_relax_init(nodei, &id, vnodeVer_unknown(), 0);
    vnodeInfo_add_addr(&nodei, addr, VSOCKADDR_UNKNOWN);

    vlock_enter(&route->lock);
    ret = node_space->ops->add_node(node_space, nodei, VADD_BY_OTHER);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}


/*
 * the routine to find 'best' suitable service from service routing space.
 * here, 'best' means resources of system that service docks is most avaiable,
 * that is to say, the service is probably the most responsive to client.
 *
 * @route:
 * @svc_hash: service hash ID.
 * @ncb: callback to show the number and protocol of service;
 * @icb: callback to show addresses.
 * @cookie: private data from user.
 */
static
int _vroute_find_service(struct vroute* route, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    DECL_VSRVC_RELAX(srvci);
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    vlock_enter(&route->lock);
    ret = srvc_space->ops->get_service(srvc_space, hash, srvci);
    vlock_leave(&route->lock);
    retE((ret < 0));

    if (!ret) { // means no services found, but need to tell client anyway.
        ncb(hash, 0, VPROTO_UNKNOWN, cookie);
        return 0;
    }
    ncb(&srvci->hash, srvci->naddrs, srvci->proto, cookie);
    for (i = 0; i < srvci->naddrs; i++) {
        icb(&srvci->hash, &srvci->addrs[i].addr, srvci->addrs[i].type, (i+1) == srvci->naddrs, cookie);
    }
    return 0;
}

/*
 * the routine to probe 'best' suitable service from neighbor nodes.
 *
 * @route:
 * @hash: service hash ID.
 * @ncb: callback to show the number of service addresses and protocol used by service
 * @icb: callback to show all addresses of service
 * @cookie: private data for user.
 */
static
int _vroute_probe_service(struct vroute* route, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_tckt_space* tckt_space = &route->tckt_space;
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

    {
        void* argv[] = {
            hash,
            ncb,
            icb,
            cookie
        };
        tckt_space->ops->add_ticket(tckt_space, VTICKET_SRVC_PROBE, argv);
    }
    vlock_leave(&route->lock);
    return 0;
}

/*
 * the routine to broadcast the service to all neighbor nodes.
 *
 * @route:
 * @srvci: service to be broadcasted.
 */
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

/*
 * the routine to reflex an address. The request from this address has
 * been sent to all inernet-accessible neighbor nodes, where all those
 * nodes have to response back it's relevant reflexive address.
 *
 * @route:
 * @addr:  address to be reflexed.
 */
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
 * the routine to probe the connectivity between current node and all active
 * neighbor nodes.
 *
 * @route:
 * @laddr: the candidate address on current node side.
 */
int _vroute_probe_connectivity(struct vroute* route, struct vsockaddr_in* laddr)
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

/*
 * the routine set inspect callback for testing module
 * this API is supposed to be uesd for testing module.
 *
 * @route:
 * @cb : callback to install
 * @cookie: private data;
 *
 */
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

/*
 * the routine to inspect route space( includes node routing space and service
 * routing space);
 * this API is supposed to be used for testing module.
 *
 * @route:
 * @nonce:
 * @insp_id:
 *
 */
void _vroute_inspect(struct vroute* route, vnonce* nonce, uint32_t insp_id)
{
    vassert(route);
    vassert(nonce);

    vlock_enter(&route->lock);
    if (route->insp_cb) {
        route->insp_cb(route, route->insp_cookie, nonce, insp_id);
    }
    vlock_leave(&route->lock);
    return ;
}

/*
 * the routine to load nodes infos from database storage when host
 * starts at first. All loaded nodes infos become potential neigbor nodes
 * in node routing space.
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
 * the routine to write back all nodes in node routing table back to database
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
 * the routine to priodically refresh the routing space.
 *
 * @route:
 */
static
int _vroute_tick(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_tckt_space* tckt_space = &route->tckt_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->tick(node_space);
    tckt_space->ops->timed_reap(tckt_space);
    vlock_leave(&route->lock);
    return 0;
}

/*
 * the routine to clean routing space, including node routing space and service
 * routing space.
 *
 * @route:
 */
static
void _vroute_clear(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    struct vroute_tckt_space* tckt_space = &route->tckt_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->clear(node_space);
    srvc_space->ops->clear(srvc_space);
    tckt_space->ops->clear(tckt_space);
    vlock_leave(&route->lock);

    return;
}

/* the routine to dump routing space, including node routing space and service
 * routing space.
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

#define DO_INSPECT(INSP_ID) do { \
        route->ops->inspect(route, &nonce, INSP_ID); \
    } while(0)

#define ADD_CHECK_TOKEN() do { \
        struct vroute_tckt_space* ts = &route->tckt_space; \
        void* argv[] = { \
            &nonce, \
        }; \
        ts->ops->add_ticket(ts, VTICKET_TOKEN_CHECK, argv); \
    } while(0)

#define DO_CHECK_TOKEN()  do { \
        struct vroute_tckt_space* ts = &route->tckt_space; \
        void* argv[] = { \
            &nonce, \
        }; \
        int ret = ts->ops->check_ticket(ts, VTICKET_TOKEN_CHECK, argv); \
        retE((!ret)); \
    } while(0)

#define DO_KICK_NODEI()  do { \
        struct vroute_node_space* ns = &route->node_space; \
        ns->ops->kick_node(ns, &fromId); \
    } while(0)

#define BEGIN_QUERY() do {    \
        void* buf = NULL;  \
        buf = vdht_buf_alloc(); \
        retE((!buf)); {

#define END_QUERY(ret) \
            ret1E((ret < 0), vdht_buf_free(buf)); \
        } \
        { \
            struct vmsg_usr msg = { \
                .addr  = to_vsockaddr_from_sin(&conn->raddr), \
                .spec  = to_vsockaddr_from_sin(&conn->laddr),  \
                .msgId = VMSG_DHT,  \
                .data  = buf,       \
                .len   = ret        \
            }; \
            ret = route->msger->ops->push(route->msger, &msg); \
            ret1E((ret < 0), vdht_buf_free(buf));              \
        } \
    } while(0)

/*
 * auxilary log utility API
 */
static
void _aux_vlog(const char* msgtype, struct sockaddr_in* addr)
{
    char buf[32];
    vassert(msgtype);
    vassert(addr);

    memset(buf, 0, 32);
    vsockaddr_strlize(addr, buf, 32);
    vlogI("send     @%-24s =>  %s", msgtype, buf);
    return ;
}

/*
 * the routine to pack a @ping query and send it.
 * @route:
 * @conn:
 */
static
int _vroute_dht_ping(struct vroute* route, vnodeConn* conn)
{
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    retS((!(route->props & PROP_PING))); //ping disabled.

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->ping(&nonce, &route->myid, buf, vdht_buf_len());
    } END_QUERY(ret);

    DO_INSPECT(VROUTE_INSP_SND_PING);
    ADD_CHECK_TOKEN();

    _aux_vlog("ping", &conn->raddr);
    return 0;
}


/*
 * the routine to pack and send back a response for @ping query to source DHT
 * node where @ping query was from.
 * @route:
 * @conn:
 * @nonce:
 * @info :
 */
static
int _vroute_dht_ping_rsp(struct vroute* route, vnodeConn* conn, vnonce* nonce, vnodeInfo* nodei)
{
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(nonce);
    vassert(nodei);
    retS((!(route->props & PROP_PING_R)));

    BEGIN_QUERY() {
        ret = route->enc_ops->ping_rsp(nonce, &route->myid, nodei, buf, vdht_buf_len());
    } END_QUERY(ret);

    _aux_vlog("ping_rsp", &conn->raddr);
    return 0;
}

/*
 * the routine to pack and send @find_node query to a DHT node to get metadata
 * of special DHT node.
 * of a given node.
 * @route:
 * @conn:
 * @target
 */
static
int _vroute_dht_find_node(struct vroute* route, vnodeConn* conn, vnodeId* targetId)
{
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(targetId);
    retS((!(route->props & PROP_FIND_NODE)));

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->find_node(&nonce, &route->myid, targetId, buf, vdht_buf_len());
    } END_QUERY(ret);

    DO_INSPECT(VROUTE_INSP_SND_FIND_NODE);
    ADD_CHECK_TOKEN();

    _aux_vlog("find_node", &conn->raddr);
    return 0;
}

/*
 * the routine to pack and send a response to @find_node query.
 * @route:
 * @conn:
 * @nonce:
 * @info:
 */
static
int _vroute_dht_find_node_rsp(struct vroute* route, vnodeConn* conn, vnonce* nonce, vnodeInfo* nodei)
{
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(nonce);
    vassert(nodei);
    retS((!(route->props & PROP_FIND_NODE_R)));

    BEGIN_QUERY() {
        ret = route->enc_ops->find_node_rsp(nonce, &route->myid, nodei, buf, vdht_buf_len());
    } END_QUERY(ret);

    _aux_vlog("find_node_rsp", &conn->raddr);
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
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(targetId);
    retS((!(route->props & PROP_FIND_CLOSEST_NODES)));

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->find_closest_nodes(&nonce, &route->myid, targetId, buf, vdht_buf_len());
    } END_QUERY(ret);

    DO_INSPECT(VROUTE_INSP_SND_FIND_CLOSEST_NODES);
    ADD_CHECK_TOKEN();

    _aux_vlog("find_closest_nodes", &conn->raddr);
    return 0;
}

/*
 * the routine to pack and send back a response to @find_closest_nodes query.
 * @route:
 * @conn:
 * @nonce:
 * @closest: array of closest nodes (closest to given id);
 */
static
int _vroute_dht_find_closest_nodes_rsp(struct vroute* route, vnodeConn* conn, vnonce* nonce, struct varray* closest)
{
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(nonce);
    vassert(closest);
    retS((!(route->props & PROP_FIND_CLOSEST_NODES_R)));

    BEGIN_QUERY() {
        ret = route->enc_ops->find_closest_nodes_rsp(nonce, &route->myid, closest, buf, vdht_buf_len());
    } END_QUERY(ret);

    _aux_vlog("find_closest_nodes_rsp", &conn->raddr);
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
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->reflex(&nonce, &route->myid, buf, vdht_buf_len());
    } END_QUERY(ret);

    DO_INSPECT(VROUTE_INSP_SND_REFLEX);
    ADD_CHECK_TOKEN();

    _aux_vlog("reflex", &conn->raddr);
    return 0;
}

/*
 * the routine to pack and send back a response to @reflex_addr query.
 * @route:
 * @conn:
 * @nonce:
 * @reflexive_addr:
 */
static
int _vroute_dht_reflex_rsp(struct vroute* route, vnodeConn* conn, vnonce* nonce, struct vsockaddr_in* reflexive_addr)
{
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(nonce);
    vassert(reflexive_addr);

    BEGIN_QUERY() {
        ret = route->enc_ops->reflex_rsp(nonce, &route->myid, reflexive_addr, buf, vdht_buf_len());
    } END_QUERY(ret);

    _aux_vlog("reflex_rsp", &conn->raddr);
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
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(targetId);

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->probe(&nonce, &route->myid, targetId, buf, vdht_buf_len());
    } END_QUERY(ret);

    _aux_vlog("probe", &conn->raddr);

    DO_INSPECT(VROUTE_INSP_SND_PROBE);
    ADD_CHECK_TOKEN();
    {
        struct vroute_tckt_space* tckt_space = &route->tckt_space;
        void* argv[] = {
            &nonce,
            targetId,
            conn,
            &route->node_space
        };
        tckt_space->ops->add_ticket(tckt_space, VTICKET_CONN_PROBE, argv);
    }
    return 0;
}

/*
 * the routine to pack and send back a response to @probe_connectivity query.
 * @route:
 * @conn:
 * @nonce:
 */
static
int _vroute_dht_probe_rsp(struct vroute* route, vnodeConn* conn, vnonce* nonce)
{
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(nonce);

    BEGIN_QUERY() {
        ret = route->enc_ops->probe_rsp(nonce, &route->myid, buf, vdht_buf_len());
    } END_QUERY(ret);

    _aux_vlog("prbe_rsp", &conn->raddr);
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
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(srvci);
    retS((!(route->props & PROP_POST_SERVICE)));

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->post_service(&nonce, &route->myid, srvci, buf, vdht_buf_len());
    }END_QUERY(ret);

    _aux_vlog("post_service", &conn->raddr);
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
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(hash);
    retS((!(route->props & PROP_FIND_SERVICE)));

    BEGIN_QUERY() {
        vnonce_make(&nonce);
        ret = route->enc_ops->find_service(&nonce, &route->myid, hash, buf, vdht_buf_len());
    }END_QUERY(ret);

    DO_INSPECT(VROUTE_INSP_SND_FIND_SERVICE);
    ADD_CHECK_TOKEN();

    _aux_vlog("find_service", &conn->raddr);
    return 0;
}

/*
 * the routine to pack and send a response to @find_service query.
 * @route:
 * @conn:
 * @nonce:
 * @srvcs:
 */
static
int _vroute_dht_find_service_rsp(struct vroute* route, vnodeConn* conn, vnonce* nonce, vsrvcInfo* srvc)
{
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(nonce);
    vassert(srvc);
    retS((!(route->props & PROP_FIND_SERVICE_R)));

    BEGIN_QUERY() {
        ret = route->enc_ops->find_service_rsp(nonce, &route->myid, srvc, buf, vdht_buf_len());
    }END_QUERY(ret);

    _aux_vlog("find_service_rsp", &conn->raddr);
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
    vassert(item);
    vnodeInfo_relax_free((vnodeInfo*)item);
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
    DECL_VNODE_RELAX(nodei);
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->ping(ctxt, &nonce, &fromId);
    retE((ret < 0));

    if (vsockaddr_is_public(&conn->raddr)) {
        vnodeInfo_relax_init(nodei, &fromId, vnodeVer_unknown(), 0);
        vnodeInfo_add_addr(&nodei, &conn->raddr, VSOCKADDR_REFLEXIVE);
        ret = node_space->ops->add_node(node_space, nodei, VADD_BY_PING);
        retE((ret < 0));
    }
    DO_INSPECT(VROUTE_INSP_RCV_PING);
    ret = route->dht_ops->ping_rsp(route, conn, &nonce, route->node->nodei);
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
    DECL_VNODE_RELAX(nodei);
    vnonce  nonce;
    int ret = 0;

    ret = route->dec_ops->ping_rsp(ctxt, &nonce, nodei);
    retE((ret < 0));
    DO_CHECK_TOKEN();

    ret = node_space->ops->add_node(node_space, nodei, VADD_BY_PING_RSP);
    retE((ret < 0));

    DO_INSPECT(VROUTE_INSP_RCV_PING_RSP);
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
    DECL_VNODE_RELAX(nodei);
    struct varray closest;
    vnodeId targetId;
    vnodeId fromId;
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_node(ctxt, &nonce, &fromId, &targetId);
    retE((ret < 0));
    DO_KICK_NODEI();

    ret = node_space->ops->find_node(node_space, &targetId, nodei);
    retE((ret < 0));
    if (ret == 1) { //means found
        ret = route->dht_ops->find_node_rsp(route, conn, &nonce, nodei);
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
    ret = route->dht_ops->find_closest_nodes_rsp(route, conn, &nonce, &closest);
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
    DECL_VNODE_RELAX(nodei);
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_node_rsp(ctxt, &nonce, &fromId, nodei);
    retE((ret < 0));
    DO_CHECK_TOKEN();
    DO_KICK_NODEI();

    ret = node_space->ops->add_node(node_space, nodei, VADD_BY_OTHER);
    retE((ret < 0));

    DO_INSPECT(VROUTE_INSP_RCV_FIND_NODE_RSP);
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
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_closest_nodes(ctxt, &nonce, &fromId, &targetId);
    retE((ret < 0));
    DO_KICK_NODEI();

    varray_init(&closest, MAX_CAPC);
    ret = node_space->ops->get_neighbors(node_space, &targetId, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    if (ret == 0) {
        varray_deinit(&closest);
        return 0; // not response if no closest nodes found.
    }

    ret = route->dht_ops->find_closest_nodes_rsp(route, conn, &nonce, &closest);
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
    struct vroute_tckt_space* tckt_space = &route->tckt_space;
    struct vroute_node_space* node_space = &route->node_space;
    struct varray closest;
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    varray_init(&closest, MAX_CAPC);
    ret = route->dec_ops->find_closest_nodes_rsp(ctxt, &nonce, &fromId, &closest);
    ret1E((ret < 0), varray_deinit(&closest));
    {
        void* argv[] = { &nonce };
        if (!tckt_space->ops->check_ticket(tckt_space, VTICKET_TOKEN_CHECK, argv)) {
            varray_zero(&closest, _aux_vnodeInfo_free, NULL);
            varray_deinit(&closest);
            return -1;
        }
    }
    DO_KICK_NODEI();

    for (i = 0; i < varray_size(&closest); i++) {
        node_space->ops->add_node(node_space, (vnodeInfo*)varray_get(&closest, i), VADD_BY_OTHER);
    }
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);

    DO_INSPECT(VROUTE_INSP_RCV_FIND_CLOSEST_NODES_RSP);
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
    struct vsockaddr_in reflexive_addr;
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->reflex(ctxt, &nonce, &fromId);
    retE((ret < 0));
    DO_KICK_NODEI();

    vsockaddr_copy(&reflexive_addr.addr, &conn->raddr);
    if (vsockaddr_is_private(&conn->raddr)) {
        reflexive_addr.type = VSOCKADDR_UPNP;
    } else {
        reflexive_addr.type = VSOCKADDR_REFLEXIVE;
    }
    vsockaddr_copy(&reflexive_addr.addr, &conn->raddr);
    ret = route->dht_ops->reflex_rsp(route, conn, &nonce, &reflexive_addr);
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
    struct vnode* node = route->node;
    struct vsockaddr_in reflexive_addr;
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->reflex_rsp(ctxt, &nonce, &fromId, &reflexive_addr);
    retE((ret < 0));
    DO_CHECK_TOKEN();
    DO_KICK_NODEI();

    ret = node->ops->reflex_addr(node, &conn->laddr, &reflexive_addr);
    retE((ret < 0));

    DO_INSPECT(VROUTE_INSP_RCV_REFLEX_RSP);
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
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->probe(ctxt, &nonce, &fromId, &targetId);
    retE((ret < 0));
    retE((!vtoken_equal(&targetId, &route->myid)));
    DO_KICK_NODEI();

    ret = route->dht_ops->probe_rsp(route, conn, &nonce);
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
    struct vroute_tckt_space* tckt_space = &route->tckt_space;
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->probe_rsp(ctxt, &nonce, &fromId);
    retE((ret < 0));
    DO_CHECK_TOKEN();
    DO_KICK_NODEI();

    {
        void* argv[] = {
            &nonce,
            &route->node_space,
        };
        tckt_space->ops->check_ticket(tckt_space, VTICKET_CONN_PROBE, argv);
    }
    DO_INSPECT(VROUTE_INSP_RCV_PROBE_RSP);
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
    DECL_VSRVC_RELAX(srvci);
    vnodeId fromId;
    vnonce  nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->post_service(ctxt, &nonce, &fromId, srvci);
    retE((ret < 0));
    DO_KICK_NODEI();

    ret = srvc_space->ops->add_service(srvc_space, srvci);
    retE((ret < 0));

    DO_INSPECT(VROUTE_INSP_RCV_POST_SERVICE);
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
    DECL_VSRVC_RELAX(srvci);
    vsrvcHash srvcHash;
    vnodeId fromId;
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(conn);
    vassert(ctxt);

    ret = route->dec_ops->find_service(ctxt, &nonce, &fromId, &srvcHash);
    retE((ret < 0));
    DO_KICK_NODEI();

    ret = srvc_space->ops->get_service(srvc_space, &srvcHash, srvci);
    retE((ret < 0));
    retS((ret == 0)); //no response if no service was found.
    ret = route->dht_ops->find_service_rsp(route, conn, &nonce, srvci);
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
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_tckt_space* tckt_space = &route->tckt_space;
    DECL_VSRVC_RELAX(srvci);
    vnodeId fromId;
    vnonce nonce;
    int ret = 0;

    vassert(route);
    vassert(ctxt);
    vassert(conn);

    ret = route->dec_ops->find_service_rsp(ctxt, &nonce, &fromId, srvci);
    retE((ret < 0));
    DO_CHECK_TOKEN();
    DO_KICK_NODEI();

    ret = srvc_space->ops->add_service(srvc_space, srvci);
    retE((ret < 0));
    //try to add dht node hosting that service into routing table, so
    //as to cache this useful node
    ret = node_space->ops->find_node_in_neighbors(node_space, &srvci->hostid);
    retE((ret < 0));

    DO_INSPECT(VROUTE_INSP_RCV_FIND_SERVICE_RSP);

    {
        void* argv[] = { srvci, };
        tckt_space->ops->check_ticket(tckt_space, VTICKET_SRVC_PROBE, argv);

    }
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

    {
        char buf[32];
        memset(buf, 0, 32);
        vsockaddr_strlize(to_sockaddr_sin(mu->addr), buf, 32);
        vlogI("received @%-24s <=  %s", vdht_get_desc(ret), buf);
    }

    vnodeConn_set_raw(&conn, to_sockaddr_sin(mu->spec), to_sockaddr_sin(mu->addr));
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
    vroute_tckt_space_init(&route->tckt_space);

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

    vroute_tckt_space_deinit(&route->tckt_space);
    vroute_node_space_deinit(&route->node_space);
    vroute_srvc_space_deinit(&route->srvc_space);
    vlock_deinit(&route->lock);
    return ;
}

