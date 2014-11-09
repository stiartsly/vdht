#include "vglobal.h"
#include "vhost.h"

static
int _vhost_start(struct vhost* host)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);
    vassert(node);

    ret = node->ops->start(node);
    retE((ret < 0));
    vlogI(printf("host started"));
    return 0;
}

static
int _vhost_stop(struct vhost* host)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);

    ret = node->ops->stop(node);
    retE((ret < 0));
    vlogI(printf("host stopped"));
    return 0;
}

static
int _vhost_join(struct vhost* host, struct sockaddr_in* wellknown_addr)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);
    vassert(wellknown_addr);

    ret = node->ops->join(node, wellknown_addr);
    retE((ret < 0));
    vlogI(printf("join a node"));
    return 0;
}

static
int _vhost_drop(struct vhost* host, struct sockaddr_in* addr)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);
    vassert(addr);

    ret = node->ops->drop(node, addr);
    retE((ret < 0));
    vlogI(printf("drop a node"));
    return 0;
}

static
int _aux_tick_cb(void* cookie)
{
    //todo;
    return 0;
}

static
int _vhost_stabilize(struct vhost* host)
{
    struct vticker* ticker = &host->ticker;
    struct vnode* node = &host->node;
    vassert(host);

    node->ops->stabilize(node);
    ticker->ops->add_cb(ticker, _aux_tick_cb, host);
    ticker->ops->start(ticker, host->tick_tmo);
    return 0;
}

static
int _vhost_dump(struct vhost* host)
{
    vassert(host);

    vdump(printf("-> HOST"));
    vdump(printf("addr: %s:%d",host->myname, host->myport));
    host->node.ops->dump(&host->node);
    host->msger.ops->dump(&host->msger);
    host->waiter.ops->dump(&host->waiter);
    vdump(printf("<- HOST"));

    return 0;
}

static
int _vhost_loop(struct vhost* host)
{
    struct vwaiter* waiter = &host->waiter;
    int ret = 0;

    vassert(host);
    vassert(waiter);

    vlogI(printf("host in laundry."));
    while(!host->to_quit) {
        ret = waiter->ops->laundry(waiter);
        if (ret < 0) {
            continue;
        }
    }
    vlogI(printf("host quited from laundry."));
    return 0;
}

static
int _vhost_req_quit(struct vhost* host)
{
    vassert(host);
    host->to_quit = 1;
    vlogI(printf("host about to quit."));
    return 0;
}

static
struct vhost_ops host_ops = {
    .start     = _vhost_start,
    .stop      = _vhost_stop,
    .join      = _vhost_join,
    .drop      = _vhost_drop,
    .stabilize = _vhost_stabilize,
    .loop      = _vhost_loop,
    .req_quit  = _vhost_req_quit,
    .dump      = _vhost_dump
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
        sz += sizeof(long);
        data = unoff_addr(um->data, sz);
        set_int32(data, um->msgId);

        sz += sizeof(uint32_t);
        data = unoff_addr(um->data, sz);
        set_uint32(data, DHT_MAGIC);

        ms = vmsg_sys_alloc();
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
    sz += sizeof(long);

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
int _aux_get_tick_tmo(struct vconfig* cfg)
{
    char buf[32];
    int tms = 0;
    int ret = 0;
    vassert(cfg);

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

struct vhost* vhost_create(struct vconfig* cfg)
{
    struct vhost* host = NULL;
    struct vsockaddr addr;
    int ret  = 0;
    vassert(cfg);

    host = (struct vhost*)malloc(sizeof(struct vhost));
    vlog((!host), elog_malloc);
    retE_p((!host));
    memset(host, 0, sizeof(*host));

    ret = vhostaddr_get_first(host->myname, HOST_SZ);
    vlog((ret < 0), elog_vhostaddr_get_first);
    ret1E_p((ret < 0), free(host));
    ret = cfg->ops->get_int(cfg, "dht.port", &host->myport);
    if (ret < 0) {
        host->myport = DEF_DHT_PORT;
    }

    ret = vsockaddr_convert(host->myname, host->myport, &addr.vsin_addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    ret1E_p((ret < 0), free(host));

    host->to_quit = 0;
    host->ops     = &host_ops;
    host->cfg     = cfg;

    ret = _aux_get_tick_tmo(cfg);
    ret1E_p((ret < 0), free(host));
    host->tick_tmo = ret;

    ret += vmsger_init (&host->msger);
    ret += vrpc_init   (&host->rpc,  &host->msger, VRPC_UDP, &addr);
    ret += vticker_init(&host->ticker);
    ret += vnode_init  (&host->node, host->cfg, &host->msger, &host->ticker, &addr.vsin_addr);
    ret += vwaiter_init(&host->waiter);
    ret += vlsctl_init (&host->lsctl, host);
    if (ret < 0) {
        vlsctl_deinit (&host->lsctl);
        vwaiter_deinit(&host->waiter);
        vnode_deinit  (&host->node);
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
    vticker_deinit(&host->ticker);
    vrpc_deinit   (&host->rpc);
    vmsger_deinit (&host->msger);

    free(host);
    return ;
}

