#include "vglobal.h"
#include "vroute.h"

#define MAX_CAPC ((int)8)

static uint32_t peer_service_prop[] = {
    PROP_RELAY,
    PROP_STUN,
    PROP_VPN,
    PROP_DDNS,
    PROP_MROUTE,
    PROP_DHASH,
    PROP_APP
};

int _aux_route_tick_cb(void* item, void* cookie)
{
    struct vroute_node_space* space = (struct vroute_node_space*)cookie;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vsrvcInfo svc;
    int ret = 0;

    vassert(route);
    vassert(addr);
    retE((what < 0));
    retE((what >= PLUGIN_BUTT));

    vlock_enter(&route->lock);
    ret = srvc_space->ops->get_srvc_node(srvc_space, what, &svc);
    vlock_leave(&route->lock);
    retE((ret < 0));

    vsockaddr_copy(addr, &svc.addr);
    return 0;
}

static
int _vroute_set_used_index(struct vroute* route, int index)
{
    vassert(route);
    retE((index < 0));

    route->used_index = index;
    return 0;
}

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

static
int _vroute_tick(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->tick(node_space);
    varray_iterate(&route->own_svcs, _aux_route_tick_cb, node_space);
    vlock_leave(&route->lock);
    return 0;
}

static
void _vroute_clear(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->clear(node_space);
    srvc_space->ops->clear(srvc_space);
    vlock_leave(&route->lock);
    return;
}

static
void _vroute_dump(struct vroute* route)
{
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space  = &route->srvc_space;
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

    node_space->ops->dump(node_space);
    srvc_space->ops->dump(srvc_space);
    vlock_leave(&route->lock);
    vdump(printf("<- ROUTE"));
    return;
}

static
int _vroute_dispatch(struct vroute* route, struct vmsg_usr* mu)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    void* ctxt = NULL;
    int ret = 0;
    vassert(route);
    vassert(mu);

    ret = dec_ops->dec(mu->data, mu->len, &ctxt);
    retE((ret >= VDHT_UNKNOWN));
    retE((ret < 0));
    vlogI(printf("received @%s", vdht_get_desc(ret)));

    ret = route->cb_ops[ret](route, &mu->addr->vsin_addr, ctxt);
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* space = &route->node_space;
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
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
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

    ret = srvc_space->ops->add_srvc_node(srvc_space, &svc);
    retE((ret < 0));
    vnodeInfo_init(&info, &addr.id, &addr.addr, peer_service_prop[svc.usage], NULL);
    ret = node_space->ops->add_node(node_space, &info);
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
    route->used_index = 0;

    vlock_init(&route->lock);
    vroute_node_space_init(&route->node_space, route, cfg, &route->own_node);
    vroute_srvc_space_init(&route->srvc_space, cfg);

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

    vroute_node_space_deinit(&route->node_space);
    vroute_srvc_space_deinit(&route->srvc_space);
    for (; i < varray_size(&route->own_svcs); i++) {
        vsrvcInfo_free((vsrvcInfo*)varray_get(&route->own_svcs, i));
    }
    varray_deinit(&route->own_svcs);
    vlock_deinit(&route->lock);
    return ;
}

