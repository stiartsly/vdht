#include "vglobal.h"
#include "vhost.h"

#define _UINT32(data) (*(uint32_t*)data)
#define _INT32(data)  (*(int*)data)

#define _SET_UINT32(data, val) ((*(uint32_t*)data) = val)
#define _SET_INT32 (data, val) ((*(int*)data) = val)

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
    //struct vhost* host = (struct vhost*)cookie;
    vassert(host);

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
    struct vrpc* rpc = NULL;
    int ret = 0;
    int rwe = 0;

    vassert(host);
    vassert(waiter);

    while(!host->to_quit) {
        ret = waiter->ops->select(waiter, &rpc, &rwe);
        if (ret < 0) {
            continue;
        }
        switch(rwe) {
        case VRPC_RCV:
            rpc->ops->rcv(rpc);
            break;
        case VRPC_SND:
            rpc->ops->snd(rpc);
            break;
        case VRPC_ERR:
            waiter->ops->remove(waiter, rpc);
            ret = rpc->ops->err(rpc);
            if (ret < 0) {
                continue;
            }
            waiter->ops->add(waiter, rpc);
            break;
        default:
            vassert(0);
        }
    }
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
    vassert(cookie);
    vassert(um);
    vassert(*sm);


    if (IS_DHT_MSG(_UINT32(um->data))){
        *sm = vmsg_sys_alloc(0);
        retE((!*sm));
        vmsg_sys_init(*sm, um->addr, um->len, um->data);
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
    vassert(cookie);
    vassert(sm);
    vassert(um);

    if (IS_DHT_MSG(_UINT32(sm->data))) {
        int msgId = _INT32(sm->data + 4);
        vmsg_usr_init(um, msgId, &sm->addr, sm->len, sm->data);
        return 0;
    }

    //todo;
    return 0;
}

struct vhost* vhost_create(const char* hostname, int port)
{
    struct vhost* host = NULL;
    struct sockaddr_in addr;
    int ret = 0;

    vassert(host);
    vassert(port > 0);

    vassert(host);
    ret = vsockaddr_convert(hostname, port, &addr);
    retE_p((ret < 0));

    vassert(host);
    host = (struct vhost*)malloc(sizeof(struct vhost));
    ret1E_p((!host), elog_malloc);
    memset(host, 0, sizeof(*host));

    strcpy(host->myname, hostname);
    host->to_quit = 0;
    host->myport  = port;
    host->ops     = &host_ops;

    vassert(host);
    ret += vmsger_init (&host->msger);
    ret += vrpc_init   (&host->rpc,  &host->msger, VRPC_UDP, &addr);
    ret += vticker_init(&host->ticker);
    ret += vnode_init  (&host->node, &host->msger, &host->ticker, &addr);
    ret += vwaiter_init(&host->waiter);
    vassert(host);
    if (ret < 0) {
        vwaiter_deinit(&host->waiter);
        vnode_deinit  (&host->node);
        vticker_deinit(&host->ticker);
        vrpc_deinit   (&host->rpc);
        vmsger_deinit (&host->msger);
        return NULL;
    }

    host->waiter.ops->add(&host->waiter, &host->rpc);
    vmsger_reg_pack_cb  (&host->msger, _aux_msg_pack_cb  , host);
    vmsger_reg_unpack_cb(&host->msger, _aux_msg_unpack_cb, host);

    vwhere();
    return host;
}

void vhost_destroy(struct vhost* host)
{
    vassert(host);

    vwaiter_deinit(&host->waiter);
    vnode_deinit  (&host->node);
    vticker_deinit(&host->ticker);
    vrpc_deinit   (&host->rpc);
    vmsger_deinit (&host->msger);

    free(host);
    return ;
}

