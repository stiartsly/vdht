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
    struct vroute_node_space* space = &route->node_space;
    vnodeInfo ni;
    int ret = 0;

    vassert(route);
    vassert(addr);

    {
        vtoken id;
        vtoken_make(&id);
        vnodeInfo_init(&ni, &id, &unknown_node_ver, 0);
        vnodeInfo_set_eaddr(&ni, addr);
    }
    vlock_enter(&route->lock);
    ret = space->ops->add_node(space, &ni);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to find best suitable service node from service routing table
 * so that local app can meet its needs with that service.
 *
 * @route:
 * @svc_hash: service hash id.
 * @addr : [out] address of service
 */
static
int _vroute_get_service(struct vroute* route, vtoken* svc_hash, struct sockaddr_in* addr)
{
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vsrvcInfo svc;
    int found = 0;

    vassert(route);
    vassert(addr);
    vassert(svc_hash);

    vlock_enter(&route->lock);
    found = srvc_space->ops->get_srvc_node(srvc_space, svc_hash, &svc);
    vlock_leave(&route->lock);
    if (found) {
        vsockaddr_copy(addr, &svc.addr);
    }
    return found;
}

static
int _vroute_post_service(struct vroute* route, vsrvcInfo* srvc)
{
    struct vroute_node_space* node_space = &route->node_space;
    int ret = 0;
    vassert(route);
    vassert(srvc);

    vlock_enter(&route->lock);
    ret = node_space->ops->broadcast(node_space, srvc);
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
    struct vroute_record_space* record_space = &route->record_space;
    struct vroute_node_space*   node_space   = &route->node_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->tick(node_space);
    vlock_leave(&route->lock);
    record_space->ops->reap(record_space);// reap all timeout records.
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
    struct vroute_record_space* record_space = &route->record_space;
    struct vroute_node_space*   node_space   = &route->node_space;
    struct vroute_srvc_space*   srvc_space   = &route->srvc_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->clear(node_space);
    srvc_space->ops->clear(srvc_space);
    vlock_leave(&route->lock);
    record_space->ops->clear(record_space);

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
    .join_node    = _vroute_join_node,
    .get_service  = _vroute_get_service,
    .post_service = _vroute_post_service,
    .load         = _vroute_load,
    .store        = _vroute_store,
    .tick         = _vroute_tick,
    .clear        = _vroute_clear,
    .dump         = _vroute_dump
};

static
struct sockaddr_in* most_efficient_addr(struct vroute* route, vnodeInfo* dest)
{
    struct vnode* node = route->node;
    vassert(route);
    vassert(dest);

    return node->ops->get_best_usable_addr(node, dest);
}

static
int _vroute_dht_ping(struct vroute* route, vnodeInfo* dest)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    retS((!(route->props & PROP_PING))); //ping disabled.

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->ping(&token, &route->node_id, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    record_space->ops->make(record_space, &token); //record this query;
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
int _vroute_dht_ping_rsp(struct vroute* route, vnodeInfo* dest, vtoken* token, vnodeInfo* info)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(info);
    retS((!(route->props & PROP_PING_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->ping_rsp(token, info, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
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
int _vroute_dht_find_node(struct vroute* route, vnodeInfo* dest, vnodeId* target)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(target);
    retS((!(route->props & PROP_FIND_NODE)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->find_node(&token, &route->node_id, target, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    record_space->ops->make(record_space, &token);
    vlogI(printf("send @find_node"));
    return 0;
}

/*
 * @route
 * @
 */
static
int _vroute_dht_find_node_rsp(struct vroute* route, vnodeInfo* dest, vtoken* token, vnodeInfo* info)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(info);
    retS((!(route->props & PROP_FIND_NODE_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_node_rsp(token, &route->node_id, info, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
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
int _vroute_dht_find_closest_nodes(struct vroute* route, vnodeInfo* dest, vnodeId* target)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(target);
    retS((!(route->props & PROP_FIND_CLOSEST_NODES)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->find_closest_nodes(&token, &route->node_id, target, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    record_space->ops->make(record_space, &token);
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
int _vroute_dht_find_closest_nodes_rsp(struct vroute* route, vnodeInfo* dest, vtoken* token, struct varray* closest)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void*  buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(closest);
    retS((!(route->props & PROP_FIND_CLOSEST_NODES_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_closest_nodes_rsp(token, &route->node_id, closest, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
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
int _vroute_dht_post_service(struct vroute* route, vnodeInfo* dest, vsrvcInfo* service)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(service);
    retS((!(route->props & PROP_POST_SERVICE)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->post_service(&token, &route->node_id, service, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
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
int _vroute_dht_post_hash(struct vroute* route, vnodeInfo* dest, vtoken* hash)
{
    vassert(route);
    vassert(dest);
    vassert(hash);
    retS((!(route->props & PROP_POST_HASH)));

    //todo;
    return 0;
}

/*
 * @route:
 * @dest :
 * @hash :
 */
static
int _vroute_dht_get_peers(struct vroute* route, vnodeInfo* dest, vtoken* hash)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(hash);
    retS((!(route->props & PROP_GET_PEERS)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->get_peers(&token, &route->node_id, hash, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    record_space->ops->make(record_space, &token);
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
int _vroute_dht_get_peers_rsp(struct vroute* route, vnodeInfo* dest, vtoken* token, struct varray* peers)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest);
    vassert(token);
    vassert(peers);
    retS((!(route->props & PROP_GET_PEERS_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->get_peers_rsp(token, &route->node_id, peers, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(most_efficient_addr(route, dest)),
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

/* the routine to call when receiving a ping dht msg.
 *
 * @route:
 * @from:  address where the msge is from;
 * @ctxt:  dht decoder context.
 */
static
int _vroute_cb_ping(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vnode* node = route->node;
    vnodeInfo self_info;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);

    ret = dec_ops->ping(ctxt);
    retE((ret < 0));
    ret = node_space->ops->add_node(node_space, from);
    retE((ret < 0));

    (void)node->ops->self(node, &self_info);
    ret = route->dht_ops->ping_rsp(route, from, token, &self_info);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a ping-rsp dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_ping_rsp(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vroute_node_space*   node_space   = &route->node_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    vnodeInfo source_info;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);
    retE((!record_space->ops->check_exist(record_space, token)));//skip non-queried response.

    ret = dec_ops->ping_rsp(ctxt, &source_info);
    retE((ret < 0));
    ret = node_space->ops->add_node(node_space, &source_info);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a find-node dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_find_node(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_node_space* space = &route->node_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray closest;
    vnodeInfo target_info;
    vnodeId target;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);

    ret = dec_ops->find_node(ctxt, &target);
    retE((ret < 0));
    ret = space->ops->add_node(space, from);
    retE((ret < 0));

    ret = space->ops->get_node(space, &target, &target_info);
    if (ret >= 0) {
        ret = route->dht_ops->find_node_rsp(route, from, token, &target_info);
        retE((ret < 0));
        return 0;
    }
    // otherwise, send @find_closest_nodes_rsp response instead.
    varray_init(&closest, MAX_CAPC);
    ret = space->ops->get_neighbors(space, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, from, token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a find-node-rsp dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_find_node_rsp(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vroute_node_space*   node_space   = &route->node_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    vnodeInfo target_info;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);
    retE((!record_space->ops->check_exist(record_space, token)));//skip non-queried response.

    ret = dec_ops->find_node_rsp(ctxt, &target_info);
    retE((ret < 0));
    ret = node_space->ops->add_node(node_space, from);
    retE((ret < 0));

    ret = node_space->ops->add_node(node_space, &target_info);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a find-closest-nodes dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_find_closest_nodes(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_node_space* space = &route->node_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray closest;
    vnodeId target;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);

    ret = dec_ops->find_closest_nodes(ctxt, &target);
    retE((ret < 0));
    ret = space->ops->add_node(space, from);
    retE((ret < 0));

    varray_init(&closest, MAX_CAPC);
    ret = space->ops->get_neighbors(space, &target, &closest, MAX_CAPC);
    ret1E((ret <= 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, from, token, &closest);
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a find-closest-nodes-rsp dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_find_closest_nodes_rsp(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_record_space* record_space = &route->record_space;
    struct vroute_node_space*   node_space   = &route->node_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct varray closest;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);
    retE((!record_space->ops->check_exist(record_space, token)));//skip non-queried response.

    varray_init(&closest, MAX_CAPC);
    ret = dec_ops->find_closest_nodes_rsp(ctxt, &closest);
    retE((ret < 0));
    ret = node_space->ops->add_node(node_space, from);
    retE((ret < 0));

    for (; i < varray_size(&closest); i++) {
        node_space->ops->add_node(node_space, (vnodeInfo*)varray_get(&closest, i));
    }
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);

    return 0;
}

/*
 * the routine to call when receving a post-service dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_post_service(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    vsrvcInfo svc;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);

    ret = dec_ops->post_service(ctxt, &svc);
    retE((ret < 0));
    ret = node_space->ops->add_node(node_space, from);
    retE((ret < 0));

    ret = srvc_space->ops->add_srvc_node(srvc_space, &svc);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to call when receving a post-hash dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_post_hash(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);

    //TODO;
    return 0;
}

/*
 * the routine to call when receving a get_peers dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_get_peers(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(token);
    vassert(ctxt);

    //todo;
    return 0;
}
/*
 * the routine to call when receving a get_peers_rsp dht msg.
 *
 * @route:
 * @from:
 * @ctxt:
 */
static
int _vroute_cb_get_peers_rsp(struct vroute* route, vnodeInfo* from, vtoken* token, void* ctxt)
{
    vassert(route);
    vassert(from);
    vassert(token);
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
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    vnodeInfo src_info;
    vnodeId   src_id;
    vtoken token;
    void* ctxt = NULL;
    int ret = 0;

    vassert(route);
    vassert(mu);

    ret = dec_ops->dec_begin(mu->data, mu->len, &token, &src_id, &ctxt);
    retE((ret >= VDHT_UNKNOWN));
    retE((ret < 0));
    vlogI(printf("received @%s", vdht_get_desc(ret)));

    vnodeInfo_init(&src_info, &src_id, &unknown_node_ver, 1);
    vnodeInfo_set_eaddr(&src_info, to_sockaddr_sin(mu->addr));

    ret = route->cb_ops[ret](route, &src_info, &token, ctxt);
    dec_ops->dec_done(ctxt);
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

int vroute_init(struct vroute* route, struct vconfig* cfg, struct vhost* host, vnodeId* my_id)
{
    vassert(route);
    vassert(host);
    vassert(my_id);

    vtoken_copy(&route->node_id, my_id);
    _aux_route_load_proto_caps(cfg, &route->props);

    vlock_init(&route->lock);
    vroute_node_space_init  (&route->node_space, route, cfg, my_id);
    vroute_srvc_space_init  (&route->srvc_space, cfg);
    vroute_record_space_init(&route->record_space);

    route->ops     = &route_ops;
    route->dht_ops = &route_dht_ops;
    route->cb_ops  = route_cb_ops;

    route->cfg   = cfg;
    route->msger = &host->msger;
    route->node  = &host->node;

    route->msger->ops->add_cb(route->msger, route, _aux_route_msg_cb, VMSG_DHT);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    vassert(route);

    vroute_record_space_deinit(&route->record_space);
    vroute_node_space_deinit(&route->node_space);
    vroute_srvc_space_deinit(&route->srvc_space);
    vlock_deinit(&route->lock);
    return ;
}

