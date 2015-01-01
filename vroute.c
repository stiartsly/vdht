#include "vglobal.h"
#include "vroute.h"

#define MAX_CAPC ((int)8)

struct vrecord {
    struct vlist list;
    vtoken token;
    time_t snd_ts;
};

static MEM_AUX_INIT(record_cache, sizeof(struct vrecord), 4);
static
struct vrecord* vrecord_alloc(void)
{
    struct vrecord* record = NULL;

    record = (struct vrecord*)vmem_aux_alloc(&record_cache);
    vlog((!record), elog_vmem_aux_alloc);
    retE_p((!record));
    return record;
}

static
void vrecord_free(struct vrecord* record)
{
    vassert(record);
    vmem_aux_free(&record_cache, record);
    return ;
}

static
void vrecord_init(struct vrecord* record, vtoken* token)
{
    vassert(record);
    vassert(token);

    vlist_init(&record->list);
    vtoken_copy(&record->token, token);
    record->snd_ts = time(NULL);
    return ;
}

static
void vrecord_dump(struct vrecord* record)
{
    vassert(record);
    vtoken_dump(&record->token);
    vdump(printf("timestamp[snd]: %s",  ctime(&record->snd_ts)));
    return ;
}

static
int _vroute_make_record(struct vroute* route, vtoken* token)
{
    struct vrecord* record = NULL;

    vassert(route);
    vassert(token);

    record = vrecord_alloc();
    vlog((!record), elog_vrecord_alloc);
    retE((!record));

    vrecord_init(record, token);
    vlock_enter(&route->record_lock);
    vlist_add_tail(&route->records, &record->list);
    vlock_leave(&route->record_lock);

    return 0;
}

static
int _vroute_check_record(struct vroute* route, vtoken* token)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(route);
    vassert(token);

    vlock_enter(&route->record_lock);
    __vlist_for_each(node, &route->records) {
        record = vlist_entry(node, struct vrecord, list);
        if (vtoken_equal(&record->token, token)) {
            vlist_del(&record->list);
            vrecord_free(record);
            found = 1;
            break;
        }
    }
    vlock_leave(&route->record_lock);
    return found;
}

static
void _vroute_reap_timeout_records(struct vroute* route)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    time_t now = time(NULL);
    vassert(route);

    vlock_enter(&route->record_lock);
    __vlist_for_each(node, &route->records) {
        record = vlist_entry(node, struct vrecord, list);
        if ((now - record->snd_ts) > route->max_record_period) {
            vlist_del(&record->list);
            vrecord_free(record);
        }
    }
    vlock_leave(&route->record_lock);
    return ;
}

static
void _vroute_clear_records(struct vroute* route)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    vassert(route);

    vlock_enter(&route->record_lock);
    while(!vlist_is_empty(&route->records)) {
        node = vlist_pop_head(&route->records);
        record = vlist_entry(node, struct vrecord, list);
        vrecord_free(record);
    }
    vlock_leave(&route->record_lock);
    return ;
}

static
void _vroute_dump_records(struct vroute* route)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    vassert(route);

    vlock_enter(&route->record_lock);
    __vlist_for_each(node, &route->records) {
        record = vlist_entry(node, struct vrecord, list);
        vrecord_dump(record);
    }
    vlock_leave(&route->record_lock);
    return ;
}

struct vroute_record_ops route_record_ops = {
    .make  = _vroute_make_record,
    .check = _vroute_check_record,
    .reap  = _vroute_reap_timeout_records,
    .clear = _vroute_clear_records,
    .dump  = _vroute_dump_records
};


static
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
    vnodeInfo node_info;
    int ret = 0;

    vassert(route);
    vassert(addr);

    vnodeInfo_init(&node_info, 0, addr, 0, 0);

    vlock_enter(&route->lock);
    ret = space->ops->add_node(space, &node_info);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to drop a node with well-known address from routing table,
 * whose ID usually is fake and trivial.
 *
 * @route: routing table.
 * @node_addr: address of node to be dropped
 *
 */
static
int _vroute_drop_node(struct vroute* route, struct sockaddr_in* addr)
{
    struct vroute_node_space* space = &route->node_space;
    int ret = 0;

    vassert(route);
    vassert(addr);

    vlock_enter(&route->lock);
    ret = space->ops->del_node(space, addr);
    vlock_leave(&route->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to register a service info (only contain meta info) as local
 * service, and the service will be published to all nodes in routing table.
 *
 * @route:
 * @svc_hash: service Id.
 * @addr:  address of service to provide.
 *
 */
static
int _vroute_reg_service(struct vroute* route, vtoken* svc_hash, struct sockaddr_in* addr)
{
    vsrvcInfo* svc = NULL;
    int found = 0;
    int i = 0;

    vassert(route);
    vassert(addr);
    vassert(svc_hash);

    vlock_enter(&route->lock);
    for (; i < varray_size(&route->own_svcs); i++){
        svc = (vsrvcInfo*)varray_get(&route->own_svcs, i);
        if (vtoken_equal(&svc->id, svc_hash) &&
            vsockaddr_equal(&svc->addr, addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        svc = vsrvcInfo_alloc();
        vlog((!svc), elog_vsrvcInfo_alloc);
        ret1E((!svc), vlock_leave(&route->lock));
        vsrvcInfo_init(svc, svc_hash, route->nice, addr);
        varray_add_tail(&route->own_svcs, svc);
        route->own_node.weight++;
    }
    vlock_leave(&route->lock);
    return 0;
}

/*
 * the routine to unregister or nulify a service info, which registered before.
 *
 * @route:
 * @svc_hash: service hash Id.
 * @addr: address of service to provide.
 *
 */
static
int _vroute_unreg_service(struct vroute* route, vtoken* svc_hash, struct sockaddr_in* addr)
{
    vsrvcInfo* svc = NULL;
    int found = 0;
    int i = 0;

    vassert(route);
    vassert(addr);
    vassert(svc_hash);

    vlock_enter(&route->lock);
    for (; i < varray_size(&route->own_svcs); i++) {
        svc = (vsrvcInfo*)varray_get(&route->own_svcs, i);
        if (vtoken_equal(&svc->id, svc_hash) &&
            vsockaddr_equal(&svc->addr, addr)) {
            found = 1;
            break;
        }
    }
    if (found) {
        svc = (vsrvcInfo*)varray_del(&route->own_svcs, i);
        vsrvcInfo_free(svc);
        route->own_node.weight--;
    }
    vlock_leave(&route->lock);
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
    int ret = 0;

    vassert(route);
    vassert(addr);
    vassert(svc_hash);

    vlock_enter(&route->lock);
    ret = srvc_space->ops->get_srvc_node(srvc_space, svc_hash, &svc);
    vlock_leave(&route->lock);
    retE((ret < 0));

    vsockaddr_copy(addr, &svc.addr);
    return 0;
}

/*
 * the routine to update the nice index ( the index of resource avaiblility)
 * of local host. The higher the nice value is, the lower availability for
 * other nodes can provide.
 *
 * @route:
 * @nice : an index of resource availability.
 *
 */
static
int _vroute_kick_nice(struct vroute* route, int32_t nice)
{
    vsrvcInfo* svc = NULL;
    int i = 0;

    vassert(route);
    vassert(nice >= 0);
    vassert(nice <= 10);

    vlock_enter(&route->lock);
    route->nice = nice;
    for (; i < varray_size(&route->own_svcs); i++) {
        svc = (vsrvcInfo*)varray_get(&route->own_svcs, i);
        svc->nice = nice;
    }
    vlock_leave(&route->lock);
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
    struct vroute_node_space* node_space = &route->node_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->tick(node_space);
    varray_iterate(&route->own_svcs, _aux_route_tick_cb, node_space);
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
    struct vroute_node_space* node_space = &route->node_space;
    struct vroute_srvc_space* srvc_space = &route->srvc_space;
    vassert(route);

    vlock_enter(&route->lock);
    node_space->ops->clear(node_space);
    srvc_space->ops->clear(srvc_space);
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
    int i = 0;
    vassert(route);

    vdump(printf("-> ROUTE"));
    vlock_enter(&route->lock);
    vdump(printf("-> MY NODE"));
    vnodeInfo_dump(&route->own_node);
    vdump(printf("<- MY NODE"));
    vdump(printf("-> LOCAL SERVICES"));
    for (; i < varray_size(&route->own_svcs); i++) {
        vsrvcInfo_dump((vsrvcInfo*)varray_get(&route->own_svcs, i));
    }
    vdump(printf("<- LOCAL SERVICES"));

    node_space->ops->dump(node_space);
    srvc_space->ops->dump(srvc_space);
    vlock_leave(&route->lock);
    vdump(printf("<- ROUTE"));
    return;
}

/*
 * the routine to dispatch dht message
 *
 * @route:
 */
static
int _vroute_dispatch(struct vroute* route, struct vmsg_usr* mu)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    void* ctxt = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(mu);

    ret = dec_ops->dec(mu->data, mu->len, &token, &ctxt);
    retE((ret >= VDHT_UNKNOWN));
    retE((ret < 0));
    retE((!route->record_ops->check(route, &token))); // skip not-my-rsp msg.
    vlogI(printf("received @%s", vdht_get_desc(ret)));

    ret = route->cb_ops[ret](route, &mu->addr->vsin_addr, ctxt);
    dec_ops->dec_done(ctxt);
    retE((ret < 0));
    return 0;
}

static
int _vroute_permit(struct vroute* route, uint32_t prop)
{
    vassert(route);

    return ((route->props & prop));
}

static
struct vroute_ops route_ops = {
    .join_node     = _vroute_join_node,
    .drop_node     = _vroute_drop_node,
    .reg_service   = _vroute_reg_service,
    .unreg_service = _vroute_unreg_service,
    .get_service   = _vroute_get_service,
    .kick_nice     = _vroute_kick_nice,
    .dsptch        = _vroute_dispatch,
    .permit        = _vroute_permit,
    .load          = _vroute_load,
    .store         = _vroute_store,
    .tick          = _vroute_tick,
    .clear         = _vroute_clear,
    .dump          = _vroute_dump
};

static
int _vroute_dht_ping(struct vroute* route, struct sockaddr_in* dest_addr)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    retS((!route->ops->permit(route, PROP_PING))); //ping disabled.

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->ping(&token, &route->own_node.id, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->record_ops->make(route, &token);
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
int _vroute_dht_ping_rsp(struct vroute* route, struct sockaddr_in* dest_addr, vtoken* token, vnodeInfo* info)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(token);
    vassert(info);
    retS((!route->ops->permit(route, PROP_PING_R))); // ping_rsp disabled.

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->ping_rsp(token, info, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
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
int _vroute_dht_find_node(struct vroute* route, struct sockaddr_in* dest_addr, vnodeId* target)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(target);
    retS((!route->ops->permit(route, PROP_FIND_NODE))); //find_node disabled.

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->find_node(&token, &route->own_node.id, target, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->record_ops->make(route, &token);
    vlogI(printf("send @find_node"));
    return 0;
}

/*
 * @route
 * @
 */
static
int _vroute_dht_find_node_rsp(struct vroute* route, struct sockaddr_in* dest_addr, vtoken* token, vnodeInfo* info)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(token);
    vassert(info);
    retS((!route->ops->permit(route, PROP_FIND_NODE_R))); //find_node_rsp disabled.

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_node_rsp(token, &route->own_node.id, info, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
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
int _vroute_dht_find_closest_nodes(struct vroute* route, struct sockaddr_in* dest_addr, vnodeId* target)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(target);
    retS((!route->ops->permit(route, PROP_FIND_CLOSEST_NODES)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->find_closest_nodes(&token, &route->own_node.id, target, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->record_ops->make(route, &token);
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
int _vroute_dht_find_closest_nodes_rsp(struct vroute* route,
        struct sockaddr_in* dest_addr,
        vtoken* token,
        struct varray* closest)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void*  buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(token);
    vassert(closest);
    retS((!route->ops->permit(route, PROP_FIND_CLOSEST_NODES_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->find_closest_nodes_rsp(token, &route->own_node.id, closest, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
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
int _vroute_dht_post_service(struct vroute* route, struct sockaddr_in* dest_addr, vsrvcInfo* service)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(service);
    retS((!route->ops->permit(route, PROP_POST_SERVICE)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->post_service(&token, &route->own_node.id, service, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
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
int _vroute_dht_post_hash(struct vroute* route, struct sockaddr_in* dest_addr, vtoken* hash)
{
    vassert(route);
    vassert(dest_addr);
    vassert(hash);
    retS((!route->ops->permit(route, PROP_POST_HASH)));

    //todo;
    return 0;
}

/*
 * @route:
 * @dest :
 * @hash :
 */
static
int _vroute_dht_get_peers(struct vroute* route, struct sockaddr_in* dest_addr, vtoken* hash)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(hash);
    retS((!route->ops->permit(route, PROP_GET_PEERS)));

    buf = vdht_buf_alloc();
    retE((!buf));

    vtoken_make(&token);
    ret = enc_ops->get_peers(&token, &route->own_node.id, hash, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
            .msgId = VMSG_DHT,
            .data  = buf,
            .len   = ret
        };
        ret = route->msger->ops->push(route->msger, &msg);
        ret1E((ret < 0), vdht_buf_free(buf));
    }
    route->record_ops->make(route, &token);
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
        struct sockaddr_in* dest_addr,
        vtoken* token,
        struct varray* peers)
{
    struct vdht_enc_ops* enc_ops = &dht_enc_ops;
    void* buf = NULL;
    int ret = 0;

    vassert(route);
    vassert(dest_addr);
    vassert(token);
    vassert(peers);
    retS((!route->ops->permit(route, PROP_GET_PEERS_R)));

    buf = vdht_buf_alloc();
    retE((!buf));

    ret = enc_ops->get_peers_rsp(token, &route->own_node.id, peers, buf, vdht_buf_len());
    ret1E((ret < 0), vdht_buf_free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(dest_addr),
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
    vnodeInfo source_info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&source_info.addr, from);
    ret = dec_ops->ping(ctxt, &token, &source_info.id);
    retE((ret < 0));
    vnodeInfo_init(&source_info, &source_info.id, &source_info.addr, NULL, 1);
    ret = space->ops->add_node(space, &source_info);
    retE((ret < 0));
    ret = route->dht_ops->ping_rsp(route, from, &token, &route->own_node);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_ping_rsp(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_node_space* space = &route->node_space;
    vnodeInfo source_info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    ret = dec_ops->ping_rsp(ctxt, &token, &source_info);
    retE((ret < 0));
    vsockaddr_copy(&source_info.addr, from);
    ret = space->ops->add_node(space, &source_info);
    retE((ret < 0));
    return 0;
}

static
int _vroute_cb_find_node(struct vroute* route, struct sockaddr_in* from, void* ctxt)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vroute_node_space* space = &route->node_space;
    struct varray closest;
    vnodeInfo source_info;
    vnodeInfo target_info;
    vnodeId target;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&source_info.addr, from);
    ret = dec_ops->find_node(ctxt, &token, &source_info.id, &target);
    retE((ret < 0));
    vnodeInfo_init(&source_info, &source_info.id, &source_info.addr, NULL, 1);
    ret = space->ops->add_node(space, &source_info);
    retE((ret < 0));

    ret = space->ops->get_node(space, &target, &target_info);
    if (ret >= 0) {
        ret = route->dht_ops->find_node_rsp(route, from, &token, &target_info);
        retE((ret < 0));
        return 0;
    }
    // otherwise, send @find_closest_nodes_rsp response instead.
    varray_init(&closest, MAX_CAPC);
    ret = space->ops->get_neighbors(space, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, from, &token, &closest);
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
    vnodeInfo target_info;
    vnodeInfo source_info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&source_info.addr, from);
    ret = dec_ops->find_node_rsp(ctxt, &token, &source_info.id, &target_info);
    retE((ret < 0));
    ret = space->ops->add_node(space, &target_info);
    retE((ret < 0));

    vnodeInfo_init(&source_info, &source_info.id, &source_info.addr, NULL, 1);
    ret = space->ops->add_node(space, &source_info);
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
    vnodeInfo source_info;
    vnodeId target;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&source_info.addr, from);
    ret = dec_ops->find_closest_nodes(ctxt, &token, &source_info.id, &target);
    retE((ret < 0));
    vnodeInfo_init(&source_info, &source_info.id, &source_info.addr, NULL, 1);
    ret = space->ops->add_node(space, &source_info);
    retE((ret < 0));

    varray_init(&closest, MAX_CAPC);
    ret = space->ops->get_neighbors(space, &target, &closest, MAX_CAPC);
    ret1E((ret < 0), varray_deinit(&closest));
    ret = route->dht_ops->find_closest_nodes_rsp(route, from, &token, &closest);
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
    vnodeInfo source_info;
    vtoken token;
    int ret = 0;
    int i = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&source_info.addr, from);
    varray_init(&closest, MAX_CAPC);
    ret = dec_ops->find_closest_nodes_rsp(ctxt, &token, &source_info.id, &closest);
    retE((ret < 0));

    for (; i < varray_size(&closest); i++) {
        space->ops->add_node(space, (vnodeInfo*)varray_get(&closest, i));
    }
    varray_zero(&closest, _aux_vnodeInfo_free, NULL);
    varray_deinit(&closest);

    vnodeInfo_init(&source_info, &source_info.id, &source_info.addr, NULL, 1);
    ret = space->ops->add_node(space, &source_info);
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
    vnodeInfo source_info;
    vtoken token;
    int ret = 0;

    vassert(route);
    vassert(from);
    vassert(ctxt);

    vsockaddr_copy(&source_info.addr, from);
    ret = dec_ops->post_service(ctxt, &token, &source_info.id, &svc);
    retE((ret < 0));
    vsockaddr_combine(from, &svc.addr, &svc.addr);
    ret = srvc_space->ops->add_srvc_node(srvc_space, &svc);
    retE((ret < 0));
    vnodeInfo_init(&source_info, &source_info.id, &source_info.addr, NULL, 1);
    ret = node_space->ops->add_node(node_space, &source_info);
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

static
int _aux_route_load_proto_caps(struct vconfig* cfg, uint32_t* props)
{
    int on = 0;
    vassert(cfg);
    vassert(props);

    *props = 0;
    cfg->inst_ops->get_ping_cap(cfg, &on);
    if (on) {
        *props |= PROP_PING;
    }
    cfg->inst_ops->get_ping_rsp_cap(cfg, &on);
    if (on) {
        *props |= PROP_PING_R;
    }
    cfg->inst_ops->get_find_node_cap(cfg, &on);
    if (on) {
        *props |= PROP_FIND_NODE;
    }
    cfg->inst_ops->get_find_node_rsp_cap(cfg, &on);
    if (on) {
        *props |= PROP_FIND_NODE_R;
    }
    cfg->inst_ops->get_find_closest_nodes_cap(cfg, &on);
    if (on) {
        *props |= PROP_FIND_CLOSEST_NODES;
    }
    cfg->inst_ops->get_find_closest_nodes_rsp_cap(cfg, &on);
    if (on) {
        *props |= PROP_FIND_CLOSEST_NODES_R;
    }
    cfg->inst_ops->get_post_service_cap(cfg, &on);
    if (on) {
        *props |= PROP_POST_SERVICE;
    }
    cfg->inst_ops->get_post_hash_cap(cfg, &on);
    if (on) {
        *props |= PROP_POST_HASH;
    }
    cfg->inst_ops->get_get_peers_cap(cfg, &on);
    if (on) {
        *props |= PROP_GET_PEERS;
    }
    cfg->inst_ops->get_get_peers_rsp_cap(cfg, &on);
    if (on) {
        *props |= PROP_GET_PEERS_R;
    }
    return 0;
}

int vroute_init(struct vroute* route, struct vconfig* cfg, struct vmsger* msger, vnodeInfo* own_info)
{
    vassert(route);
    vassert(msger);
    vassert(own_info);

    vnodeInfo_copy(&route->own_node, own_info);
    _aux_route_load_proto_caps(cfg, &route->props);
    route->nice = 5;
    varray_init(&route->own_svcs, 4);

    vlock_init(&route->lock);
    vroute_node_space_init(&route->node_space, route, cfg, &route->own_node);
    vroute_srvc_space_init(&route->srvc_space, cfg);

    route->max_record_period = 5;
    vlist_init(&route->records);
    vlock_init(&route->record_lock);
    route->record_ops = &route_record_ops;

    route->ops     = &route_ops;
    route->dht_ops = &route_dht_ops;
    route->cb_ops  = route_cb_ops;

    route->cfg   = cfg;
    route->msger = msger;

    msger->ops->add_cb(msger, route, _aux_route_msg_cb, VMSG_DHT);
    return 0;
}

void vroute_deinit(struct vroute* route)
{
    int i = 0;
    vassert(route);

    route->record_ops->clear(route);
    vlock_deinit(&route->record_lock);

    vroute_node_space_deinit(&route->node_space);
    vroute_srvc_space_deinit(&route->srvc_space);
    for (; i < varray_size(&route->own_svcs); i++) {
        vsrvcInfo_free((vsrvcInfo*)varray_get(&route->own_svcs, i));
    }
    varray_deinit(&route->own_svcs);
    vlock_deinit(&route->lock);
    return ;
}

