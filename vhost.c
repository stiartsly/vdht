#include "vglobal.h"
#include "vhost.h"

static
int _vhost_start(struct vhost* host)
{
    struct vnode* node = &host->node;
    vassert(host);
    vassert(node);

    return node->ops->start(node);
}

static
int _vhost_stop(struct vhost* host)
{
    struct vnode* node = &host->node;
    vassert(host);

    return node->ops->stop(node);
}

static
int _vhost_join(struct vhost* host, struct sockaddr_in* wellknown_addr)
{
    struct vnode* node = &host->node;

    vassert(host);
    vassert(wellknown_addr);

    return node->ops->join(node, wellknown_addr);
}

static
int _vhost_drop(struct vhost* host, struct sockaddr_in* addr)
{
    struct vnode* node = &host->node;
    vassert(host);
    vassert(addr);

    return node->ops->drop(node, addr);
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
    ticker->ops->start(ticker, 5);
    return 0;
}

static
int _vhost_dump(struct vhost* host)
{
    vassert(host);

    //todo;
    return 0;
}

static
int _vhost_loop(struct vhost* host)
{
    struct vwaiter* waiter = &host->waiter;
    int ret = 0;

    vassert(host);
    vassert(waiter);

    while(!host->to_quit) {
        ret = waiter->ops->laundry(waiter);
        if (ret < 0) {
            continue;
        }
    }
    return 0;
}

static
int _vhost_req_quit(struct vhost* host)
{
    vassert(host);
    host->to_quit = 1;
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
    vassert(cookie);
    vassert(um);
    vassert(*sm);

    if (VDHT_MSG(um->msgId)) {
        set_uint32(um->data, DHT_MAGIC);
        set_int32(offset_addr(um->data, sizeof(unsigned long)), um->msgId);

        ms = vmsg_sys_alloc(0);
        vlog((!ms), elog_vmsg_sys_alloc);
        retE((!ms));
        vmsg_sys_init(ms, um->addr, um->len, um->data);
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
    uint32_t magic = 0;
    int msgId = 0;
    vassert(cookie);
    vassert(sm);
    vassert(um);

    magic = get_uint32(sm->data);
    msgId = get_int32(offset_addr(sm->data, sizeof(uint32_t)));

    if (IS_DHT_MSG(magic)) {
        vmsg_usr_init(um, msgId, &sm->addr, sm->len-8, sm->data);
        return 0;
    }
    if (IS_PLUG_MSG(magic)) {
        vmsg_usr_init(um, msgId, &sm->addr, sm->len-8, sm->data);
        return 0;
    }

    //todo;
    return 0;
}

struct vhost* vhost_create(struct vconfig* cfg, const char* hostname, int port)
{
    struct vhost* host = NULL;
    struct vsockaddr addr;
    int ret = 0;

    vassert(cfg);
    vassert(hostname);
    vassert(port > 0);

    ret = vsockaddr_convert(hostname, port, &addr.vsin_addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE_p((ret < 0));

    host = (struct vhost*)malloc(sizeof(struct vhost));
    vlog((!host), elog_malloc);
    retE_p((!host));
    memset(host, 0, sizeof(*host));

    strcpy(host->myname, hostname);
    host->to_quit = 0;
    host->myport  = port;
    host->ops     = &host_ops;
    host->cfg     = cfg;

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

