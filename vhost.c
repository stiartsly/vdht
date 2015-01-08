#include "vglobal.h"
#include "vhost.h"

/*
 * the routine to start host asynchronizedly.
 * @host: handle to host.
 */
static
int _vhost_start(struct vhost* host)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);

    ret = node->ops->start(node);
    retE((ret < 0));
    vlogI(printf("Host started"));
    return 0;
}

/*
 * the routine to stop host asynchronizedly.
 * @host:
 */
static
int _vhost_stop(struct vhost* host)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);

    ret = node->ops->stop(node);
    retE((ret < 0));
    vlogI(printf("Host stopped"));
    return 0;
}

/*
 * the routine to add wellknown node to route table.
 * @host:
 * @wellknown_addr: address of node to join.
 */
static
int _vhost_join(struct vhost* host, struct sockaddr_in* wellknown_addr)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(wellknown_addr);
    retE((vnodeInfo_has_addr(&host->own_node_info, wellknown_addr)));

    ret = route->ops->join_node(route, wellknown_addr);
    retE((ret < 0));
    vlogI(printf("Joined a node"));
    return 0;
}

/*
 * the routine to drop given node out of route table.
 * @host:
 * @addr: address of node to drop.
 */
static
int _vhost_drop(struct vhost* host, struct sockaddr_in* addr)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(addr);

    retE((vnodeInfo_has_addr(&host->own_node_info, addr)));

    ret = route->ops->drop_node(route, addr);
    retE((ret < 0));
    vlogI(printf("Dropped a node"));
    return 0;
}

static
int _aux_host_tick_cb(void* cookie)
{
    struct vhost*   host   = (struct vhost*)cookie;
    struct vkicker* kicker = &host->kicker;
    int ret = 0;

    vassert(host);

    ret = kicker->ops->kick_nice(kicker);
    retE((ret < 0));

    //todo;

    return 0;
}

/*
 * the routine to register the periodical work with certian interval
 * to the ticker, which is running peridically at background in the
 * form of timer.
 * @host:
 */
static
int _vhost_stabilize(struct vhost* host)
{
    struct vticker* ticker = &host->ticker;
    struct vnode*    node  = &host->node;
    int ret = 0;

    vassert(host);

    ret = node->ops->stabilize(node);
    retE((ret < 0));
    ret = ticker->ops->add_cb(ticker, _aux_host_tick_cb, host);
    retE((ret < 0));
    ret = ticker->ops->start(ticker, host->tick_tmo);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to dump all infomation about host.
 * @host:
 */
static
void _vhost_dump(struct vhost* host)
{
    vassert(host);

    vdump(printf("-> HOST"));
    vsockaddr_dump(&host->own_node_info.laddr);
    host->route.ops->dump(&host->route);
    host->node.ops->dump(&host->node);
    host->msger.ops->dump(&host->msger);
    host->waiter.ops->dump(&host->waiter);
    vdump(printf("<- HOST"));

    return;
}

static
int _aux_host_loop_entry(void* argv)
{
    struct vhost*   host   = (struct vhost*)argv;
    struct vwaiter* waiter = &host->waiter;
    int ret = 0;

    vassert(host);

    vlogI(printf("Host laundrying"));
    while(!host->to_quit) {
        ret = waiter->ops->laundry(waiter);
        if (ret < 0) {
            continue;
        }
    }
    vlogI(printf("Host quited from laundrying"));
    return 0;
}

/*
 * todo:
 * @host:
 */
static
int _vhost_daemonize(struct vhost* host)
{
    int ret = 0;
    vassert(host);

    host->to_quit = 0;
    ret = vthread_init(&host->thread, _aux_host_loop_entry, host);
    vlog((ret < 0), elog_vthread_init);
    retE((ret < 0));

    vthread_start(&host->thread);
    return 0;
}

/*
 * the routine to reqeust to quit from laundry loop and wait for
 * thread to quit.
 * @host:
 */
static
int _vhost_shutdown(struct vhost* host)
{
    int exit_code = 0;
    vassert(host);

    host->to_quit = 1;
    vthread_join(&host->thread, &exit_code);

    vlogI(printf("Host Exited"));
    return 0;
}

/* the routine to get version
 * @host:
 * @buf : buffer to store the version.
 * @len : buffer length.
 */
static
char* _vhost_get_version(struct vhost* host)
{
    /*
     * version : a.b.c.d.e
     * a: major
     * b: minor
     * c: bugfix
     * d: demo(candidate)
     * e: skipped.
     */
    const char* version = "0.0.0.1.0";
    vassert(host);

    return (char*)version;
}

/*
 * the routine to send bogus query to dest node with give address. It only
 * can be used as debug usage.
 * @host:
 * @what: what dht query;
 * @dest: dest address of dht query.
 *
 */
static
int _vhost_bogus_query(struct vhost* host, int what, struct sockaddr_in* dest_addr)
{
    struct vroute* route = &host->route;
    vnodeInfo dest_node;
    vnodeId   dest_id;
    int ret = 0;

    vassert(host);
    vassert(dest_addr);

    vtoken_make(&dest_id);
    vnodeInfo_init(&dest_node, &dest_id, dest_addr, &zero_node_ver, 0);

    switch(what) {
    case VDHT_PING:
        ret = route->dht_ops->ping(route, &dest_node);
        break;

    case VDHT_FIND_NODE:
        ret = route->dht_ops->find_node(route, &dest_node, &host->own_node_info.id);
        break;

    case VDHT_FIND_CLOSEST_NODES:
        ret = route->dht_ops->find_closest_nodes(route, &dest_node, &host->own_node_info.id);
        break;

    case VDHT_POST_SERVICE:
    case VDHT_POST_HASH:
    case VDHT_GET_PEERS:
    default:
        retE((1));
        break;
    }
    retE((ret < 0));
    return 0;
}

static
struct vhost_ops host_ops = {
    .start       = _vhost_start,
    .stop        = _vhost_stop,
    .join        = _vhost_join,
    .drop        = _vhost_drop,
    .stabilize   = _vhost_stabilize,
    .daemonize   = _vhost_daemonize,
    .shutdown    = _vhost_shutdown,
    .dump        = _vhost_dump,
    .get_version = _vhost_get_version,
    .bogus_query = _vhost_bogus_query
};

/* the routine to plug a plugin server, which will be added into route
 * table.
 * @host:
 * @waht: plugin ID.
 */
static
int _vhost_publish_service(struct vhost* host, vtoken* svc_hash, struct sockaddr_in* addr)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(addr);
    vassert(svc_hash);

    ret = route->ops->reg_service(route, svc_hash, addr);
    retE((ret < 0));
    return 0;
}

/* the routine to unplug a plugin server, which will be removed out of
 * route table.
 * @host:
 * @waht: plugin ID.
 */
static
int _vhost_cancel_service(struct vhost* host, vtoken* svc_hash, struct sockaddr_in* addr)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(addr);
    vassert(svc_hash);

    ret = route->ops->unreg_service(route, svc_hash, addr);
    retE((ret < 0));
    return 0;
}

static
int _vhost_get_service_metainfo(struct vhost* host, vtoken* svc_hash, struct sockaddr_in* addr)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(addr);
    vassert(svc_hash);

    ret = route->ops->get_service(route, svc_hash, addr);
    retE((ret < 0));
    return 0;
}

struct vhost_svc_ops host_svc_ops = {
    .publish = _vhost_publish_service,
    .cancel  = _vhost_cancel_service,
    .get     = _vhost_get_service_metainfo
};

/*
 * @cookie
 * @um: [in]  usr msg format
 * @sm: [out] sys msg format
 *
 * 1. dht msg format (usr msg is same as sys msg.)
 * ----------------------------------------------------------
 * |<- dht magic ->|<- msgId ->|<-- dht msg content. -----|
 * ----------------------------------------------------------
 *
 * 2. other msg format
 *
 */
static
int _aux_vhost_pack_msg_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys* sm)
{
    char* data = NULL;
    int sz = 0;

    vassert(cookie);
    vassert(um);
    vassert(sm);

    if (VDHT_MSG(um->msgId)) {
        sz += sizeof(int32_t);
        data = unoff_addr(um->data, sz);
        set_int32(data, um->msgId);

        sz += sizeof(uint32_t);
        data = unoff_addr(um->data, sz);
        set_uint32(data, DHT_MAGIC);

        vmsg_sys_init(sm, um->addr, um->len + sz, data);
        return 0;
    }
    //todo;
    return -1;
}

/*
 * @cookie:
 * @sm:[in] sys msg format
 * @um:[out]usr msg format
 *
 */
static
int _aux_vhost_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    void* data = sm->data;
    uint32_t magic = 0;
    int msgId = 0;
    int sz = 0;

    vassert(cookie);
    vassert(sm);
    vassert(um);

    magic = get_uint32(data);
    sz += sizeof(uint32_t);

    data = offset_addr(sm->data, sz);
    msgId = get_int32(data);
    sz += sizeof(int32_t);

    data = offset_addr(sm->data, sz);
    if (IS_DHT_MSG(magic)) {
        vmsg_usr_init(um, msgId, &sm->addr, sm->len-sz, data);
        return 0;
    }
    //todo;
    return 0;
}

static
int _aux_vhost_set_addr(struct vconfig* cfg, struct sockaddr_in* addr)
{
    char ip[64];
    int port = 0;
    int ret  = 0;
    vassert(cfg);
    vassert(addr);

    memset(ip, 0, 64);
    ret = vhostaddr_get_first(ip, 64);
    vlog((ret < 0), elog_vhostaddr_get_first);
    retE((ret < 0));
    ret = cfg->inst_ops->get_dht_port(cfg, &port);
    retE((ret < 0));
    ret = vsockaddr_convert(ip, (uint16_t)port, addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));

    return 0;
}

struct vhost* vhost_create(struct vconfig* cfg)
{
    struct vhost* host = NULL;
    int tmo = 0;
    int ret = 0;
    vassert(cfg);

    ret = cfg->inst_ops->get_host_tick_tmo(cfg, &tmo);
    retE_p((ret < 0));

    host = (struct vhost*)malloc(sizeof(struct vhost));
    vlog((!host), elog_malloc);
    retE_p((!host));
    memset(host, 0, sizeof(*host));

    host->tick_tmo = tmo;
    host->to_quit  = 0;
    host->cfg      = cfg;
    host->ops      = &host_ops;
    host->svc_ops  = &host_svc_ops;

    {
        vnodeId  nid;
        vnodeVer ver;
        struct sockaddr_in addr;

        vtoken_make(&nid);
        ret = _aux_vhost_set_addr(cfg, &addr);
        ret1E_p((ret < 0), free(host));

        vnodeVer_unstrlize(host->ops->get_version(host), &ver);
        vnodeInfo_init(&host->own_node_info, &nid, &addr, &ver, 0);
    }

    ret += vmsger_init (&host->msger);
    ret += vrpc_init   (&host->rpc,  &host->msger, VRPC_UDP, to_vsockaddr_from_sin(&host->own_node_info.laddr));
    ret += vticker_init(&host->ticker);
    ret += vroute_init (&host->route, cfg, &host->msger, &host->own_node_info);
    ret += vnode_init  (&host->node,  cfg, &host->ticker,&host->route);
    ret += vwaiter_init(&host->waiter);
    ret += vlsctl_init (&host->lsctl, host, cfg);
    ret += vkicker_init(&host->kicker, &host->route, cfg);
    ret += vstunc_init (&host->stunc, &host->msger, &host->ticker, &host->route);
    ret += vhashgen_init(&host->hashgen);
    if (ret < 0) {
        vhashgen_deinit(&host->hashgen);
        vstunc_deinit  (&host->stunc);
        vkicker_deinit (&host->kicker);
        vlsctl_deinit  (&host->lsctl);
        vwaiter_deinit (&host->waiter);
        vnode_deinit   (&host->node);
        vroute_deinit  (&host->route);
        vticker_deinit (&host->ticker);
        vrpc_deinit    (&host->rpc);
        vmsger_deinit  (&host->msger);
        free(host);
        return NULL;
    }

    host->waiter.ops->add(&host->waiter, &host->rpc);
    host->waiter.ops->add(&host->waiter, &host->lsctl.rpc);
    vmsger_reg_pack_cb  (&host->msger, _aux_vhost_pack_msg_cb  , host);
    vmsger_reg_unpack_cb(&host->msger, _aux_vhost_unpack_msg_cb, host);

    return host;
}

void vhost_destroy(struct vhost* host)
{
    vassert(host);

    {
        int exit_code = 0;
        vthread_join(&host->thread, &exit_code);
    }

    host->waiter.ops->remove(&host->waiter, &host->lsctl.rpc);
    host->waiter.ops->remove(&host->waiter, &host->rpc);

    vhashgen_deinit(&host->hashgen);
    vstunc_deinit (&host->stunc);
    vkicker_deinit(&host->kicker);
    vlsctl_deinit (&host->lsctl);
    vwaiter_deinit(&host->waiter);
    vnode_deinit  (&host->node);
    vroute_deinit (&host->route);
    vticker_deinit(&host->ticker);
    vrpc_deinit   (&host->rpc);
    vmsger_deinit (&host->msger);

    free(host);
    return ;
}

