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
    vassert(node);

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
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);
    vassert(wellknown_addr);

    ret = node->ops->join(node, wellknown_addr);
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
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);
    vassert(addr);

    ret = node->ops->drop(node, addr);
    retE((ret < 0));
    vlogI(printf("Dropped a node"));
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
    struct vnode* node = &host->node;
    vassert(host);

    node->ops->stabilize(node);
    //ticker->ops->add_cb(ticker, _aux_tick_cb, host);
    ticker->ops->start(ticker, host->tick_tmo);
    return 0;
}

/* the routine to plug a plugin server, which will be added into route
 * table.
 * @host:
 * @waht: plugin ID.
 */
static
int _vhost_plug(struct vhost* host, int what)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(what >= 0);
    vassert(what < PLUGIN_BUTT);

    ret = route->plugin_ops->plug(route, what);
    retE((ret < 0));
    return 0;
}

/* the routine to unplug a plugin server, which will be removed out of
 * route table.
 * @host:
 * @waht: plugin ID.
 */
static
int _vhost_unplug(struct vhost* host, int what)
{
    struct vroute* route = &host->route;
    int ret = 0;

    vassert(host);
    vassert(what >= 0);
    vassert(what < PLUGIN_BUTT);

    ret = route->plugin_ops->unplug(route, what);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to dump all infomation about host.
 * @host:
 */
static
int _vhost_dump(struct vhost* host)
{
    char ver[64];
    vassert(host);

    vdump(printf("-> HOST"));
    memset(ver, 0, 64);
    host->ops->version(host, ver, 64);
    vdump(printf("version: %s", ver));

    vsockaddr_dump(&host->ownId.addr);
    host->route.ops->dump(&host->route);
    host->node.ops->dump(&host->node);
    host->msger.ops->dump(&host->msger);
    host->waiter.ops->dump(&host->waiter);
    vdump(printf("<- HOST"));

    return 0;
}

/*
 * the routine to get into a loop of rpc laundry
 * @host:
 */
static
int _vhost_loop(struct vhost* host)
{
    struct vwaiter* waiter = &host->waiter;
    int ret = 0;

    vassert(host);
    vassert(waiter);

    vlogI(printf("Host in laundry loop"));
    while(!host->to_quit) {
        ret = waiter->ops->laundry(waiter);
        if (ret < 0) {
            continue;
        }
    }
    vlogI(printf("Host quited from laundry loop"));
    return 0;
}

/*
 * the routine to reqeust to quit from laundry loop
 * @host:
 */
static
int _vhost_req_quit(struct vhost* host)
{
    vassert(host);
    host->to_quit = 1;
    vlogI(printf("Host about to quit."));
    return 0;
}

/* the routine to get version
 * @host:
 * @buf : buffer to store the version.
 * @len : buffer length.
 */
static
int _vhost_version(struct vhost* host, char* buf, int len)
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
    vassert(buf);

    retE((len < strlen(version) + 1));
    strcpy(buf, version);
    return 0;
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
int _vhost_bogus_query(struct vhost* host, int what, struct sockaddr_in* dest)
{
    struct vroute_dht_ops* ops = host->route.dht_ops;
    vnodeAddr nodeAddr;
    int ret = 0;

    vassert(host);
    vassert(dest);

    vnodeId_make(&nodeAddr.id);
    vsockaddr_copy(&nodeAddr.addr, dest);

    switch(what) {
    case VDHT_PING:
        ret = ops->ping(&host->route, &nodeAddr);
        break;

    case VDHT_FIND_NODE:
        ret = ops->find_node(&host->route, &nodeAddr, &host->ownId.id);
        break;

    case VDHT_FIND_CLOSEST_NODES:
        ret = ops->find_closest_nodes(&host->route, &nodeAddr, &host->ownId.id);
        break;

    case VDHT_GET_PEERS:
    case VDHT_POST_HASH:
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
    .plug        = _vhost_plug,
    .unplug      = _vhost_unplug,
    .loop        = _vhost_loop,
    .req_quit    = _vhost_req_quit,
    .dump        = _vhost_dump,
    .version     = _vhost_version,
    .bogus_query = _vhost_bogus_query
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
int _aux_msg_pack_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys** sm)
{
    struct vmsg_sys* ms = NULL;
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

        ms = vmsg_sys_alloc(0);
        vlog((!ms), elog_vmsg_sys_alloc);
        retE((!ms));
        vmsg_sys_init(ms, um->addr, um->len + sz, data);
        *sm = ms;
        return 0;
    }
    //todo;
    return 0;
}

/*
 * @cookie:
 * @sm:[in] sys msg format
 * @um:[out]usr msg format
 *
 */
static
int _aux_msg_unpack_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
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
    if (IS_PLUG_MSG(magic)) {
        vmsg_usr_init(um, msgId, &sm->addr, sm->len-sz, data);
        return 0;
    }

    //todo;
    return 0;
}

static
int _aux_init_tick_tmo(struct vconfig* cfg)
{
    char buf[32];
    int tms = 0;
    int ret = 0;
    vassert(cfg);

    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "global.tick_tmo", buf, 32);
    vcall_cond((ret < 0), strcpy(buf, DEF_HOST_TICK_TMO));
    ret = strlen(buf);
    retE((ret <= 1));

    switch(buf[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        retE((1));
        break;
    }
    errno = 0;
    ret = strtol(buf, NULL, 10);
    retE((errno));

    return (ret * tms);
}

static
int _aux_init_ownId(struct vconfig* cfg, vnodeAddr* nodeAddr)
{
    char ip[64];
    int port = 0;
    int ret  = 0;
    vassert(cfg);
    vassert(nodeAddr);

    memset(ip, 0, 64);
    ret = vhostaddr_get_first(ip, 64);
    vlog((ret < 0), elog_vhostaddr_get_first);
    retE((ret < 0));
    ret = cfg->ops->get_int(cfg, "dht.port", &port);
    if (ret < 0) {
        port = DEF_DHT_PORT;
    }
    ret = vsockaddr_convert(ip, port, &nodeAddr->addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));

    vnodeId_make(&nodeAddr->id);
    return 0;
}

static
int _aux_init_ver(struct vhost* host, vnodeVer* ver)
{
    char str_ver[64];
    vassert(host);

    memset(str_ver, 0, 64);
    host->ops->version(host, str_ver, 64);
    vnodeVer_unstrlize(str_ver, ver);

    return 0;
}

struct vhost* vhost_create(struct vconfig* cfg)
{
    struct vhost* host = NULL;
    vnodeAddr nodeAddr;
    vnodeVer ver;
    int ret = 0;
    int tmo = 0;
    vassert(cfg);

    ret = _aux_init_ownId(cfg, &nodeAddr);
    retE_p((ret < 0));
    tmo = _aux_init_tick_tmo(cfg);
    retE_p((tmo < 0));

    host = (struct vhost*)malloc(sizeof(struct vhost));
    vlog((!host), elog_malloc);
    retE_p((!host));
    memset(host, 0, sizeof(*host));

    vnodeAddr_copy(&host->ownId, &nodeAddr);
    host->tick_tmo = tmo;
    host->to_quit  = 0;
    host->cfg = cfg;
    host->ops = &host_ops;
    _aux_init_ver(host, &ver);

    ret += vmsger_init (&host->msger);
    ret += vrpc_init   (&host->rpc,  &host->msger, VRPC_UDP, to_vsockaddr_from_sin(&nodeAddr.addr));
    ret += vticker_init(&host->ticker);
    ret += vroute_init (&host->route, cfg, &host->msger, &nodeAddr, &ver);
    ret += vnode_init  (&host->node,  cfg, &host->ticker,&host->route, &nodeAddr);
    ret += vwaiter_init(&host->waiter);
    ret += vlsctl_init (&host->lsctl, host);
    if (ret < 0) {
        vlsctl_deinit (&host->lsctl);
        vwaiter_deinit(&host->waiter);
        vnode_deinit  (&host->node);
        vroute_deinit (&host->route);
        vticker_deinit(&host->ticker);
        vrpc_deinit   (&host->rpc);
        vmsger_deinit (&host->msger);
        free(host);
        return NULL;
    }

    host->waiter.ops->add(&host->waiter, &host->rpc);
    host->waiter.ops->add(&host->waiter, &host->lsctl.rpc);
    vmsger_reg_pack_cb  (&host->msger, _aux_msg_pack_cb  , host);
    vmsger_reg_unpack_cb(&host->msger, _aux_msg_unpack_cb, host);

    return host;
}

void vhost_destroy(struct vhost* host)
{
    vassert(host);

    host->waiter.ops->remove(&host->waiter, &host->lsctl.rpc);
    host->waiter.ops->remove(&host->waiter, &host->rpc);

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

