#include "vglobal.h"
#include "vnode.h"

enum vnode_mode {
    VDHT_OFF,
    VDHT_UP,
    VDHT_RUN,
    VDHT_DOWN,
    VDHT_ERR,
    VDHT_M_BUTT
};

static
char* node_mode_desc[] = {
    "offline",
    "up",
    "running",
    "down",
    "error",
    NULL
};

/*
 * start current dht node.
 * @node:
 */
static
int _vnode_start(struct vnode* node)
{
    struct vupnpc* upnpc = &node->upnpc;
    int ret = 0;
    vassert(node);

    ret = upnpc->ops->setup(upnpc);
    retE((ret < 0));

    vlock_enter(&node->lock);
    if (node->mode != VDHT_OFF) {
        vlock_leave(&node->lock);
        return 0;
    }
    node->mode = VDHT_UP;
    vlock_leave(&node->lock);
    return 0;
}

/*
 * @node
 */
static
int _vnode_stop(struct vnode* node)
{
    struct vupnpc* upnpc = &node->upnpc;
    vassert(node);

    vlock_enter(&node->lock);
    if ((node->mode == VDHT_OFF) || (node->mode == VDHT_ERR)) {
        vlock_leave(&node->lock);
        return 0;
    }
    node->mode = VDHT_DOWN;
    vlock_leave(&node->lock);

    upnpc->ops->shutdown(upnpc);
    return 0;
}

static
int _vnode_join(struct vnode* node)
{
    vassert(node);

    vlock_enter(&node->lock);
    while (node->mode != VDHT_OFF) {
        vlock_leave(&node->lock);
        sleep(1);
        vlock_enter(&node->lock);
    }
    vlock_leave(&node->lock);

    return 0;
}

static
int _aux_node_tick_cb(void* cookie)
{
    struct vnode* node    = (struct vnode*)cookie;
    struct vupnpc* upnpc = &node->upnpc;
    struct vroute* route = node->route;
    time_t now = time(NULL);
    vassert(node);

    vlock_enter(&node->lock);
    switch (node->mode) {
    case VDHT_OFF:
    case VDHT_ERR:  {
        //do nothing;
        break;
    }
    case VDHT_UP: {
        struct sockaddr_in uaddr;

        if (upnpc->ops->map(upnpc, node->iport, node->eport, UPNP_PROTO_UDP, &uaddr) >= 0) {
            vsockaddr_copy(&node->node_info.uaddr, &uaddr);
        }

        if (route->ops->load(route) < 0) {
            node->mode = VDHT_ERR;
            break;
        }
        node->ts   = now;
        node->mode = VDHT_RUN;
        vlogI(printf("DHT start running"));
        break;
    }
    case VDHT_RUN: {
        if (now - node->ts > node->tick_interval) {
            route->ops->tick(route);
            node->ts = now;
        }
        node->svc_ops->post(node);
        break;
    }
    case VDHT_DOWN: {
        upnpc->ops->unmap(upnpc, node->eport, UPNP_PROTO_UDP);
        node->route->ops->store(node->route);
        node->mode = VDHT_OFF;
        vlogI(printf("DHT become offline"));
        break;
    }
    default:
        vassert(0);
    }
    vlock_leave(&node->lock);
    return 0;
}

/*
 * @node
 */
static
int _vnode_stabilize(struct vnode* node)
{
    struct vticker* ticker = node->ticker;
    int ret = 0;

    vassert(node);
    vassert(ticker);

    ret = ticker->ops->add_cb(ticker, _aux_node_tick_cb, node);
    retE((ret < 0));
    return 0;
}

/*
 * @node:
 */
static
int _vnode_dump(struct vnode* node)
{
    vassert(node);

    vdump(printf("-> NODE"));
    vlock_enter(&node->lock);
    vdump(printf("state:%s", node_mode_desc[node->mode]));
    vdump(printf("-> NODE_INFO"));
    vnodeInfo_dump(&node->node_info);
    vdump(printf("<- NODE_INFO"));
    node->svc_ops->dump(node);
    vlock_leave(&node->lock);
    vdump(printf("<- NODE"));

    return 0;
}

/*
 * @node
 */
static
struct sockaddr_in* _vnode_get_best_usable_addr(struct vnode* node, vnodeInfo* dest)
{
    struct sockaddr_in* addr = NULL;
    vassert(node);
    vassert(dest);

    vlock_enter(&node->lock);
    if (!vsockaddr_equal(&node->node_info.eaddr, &dest->eaddr)) {
        addr = &dest->eaddr;
    } else if (!vsockaddr_equal(&node->node_info.uaddr, &dest->uaddr)) {
        addr = &dest->uaddr;
    } else {
        addr = &dest->laddr;
    }
    vlock_leave(&node->lock);
    return addr;
}

static
void _vnode_get_own_node_info(struct vnode* node, vnodeInfo* node_info)
{
    vassert(node);
    vassert(node_info);

    vlock_enter(&node->lock);
    vnodeInfo_copy(node_info, &node->node_info);
    vlock_leave(&node->lock);

    return ;
}

static
struct vnode_ops node_ops = {
    .start                = _vnode_start,
    .stop                 = _vnode_stop,
    .join                 = _vnode_join,
    .stabilize            = _vnode_stabilize,
    .dump                 = _vnode_dump,
    .get_best_usable_addr = _vnode_get_best_usable_addr,
    .get_own_node_info    = _vnode_get_own_node_info

};

/*
 * the routine to register a service info (only contain meta info) as local
 * service, and the service will be published to all nodes in routing table.
 *
 * @node:
 * @srvcId: service Id.
 * @addr:  address of service to provide.
 *
 */
static
int _vnode_service_register(struct vnode* node, vsrvcId* srvcId, struct sockaddr_in* addr)
{
    vsrvcInfo* svc = NULL;
    int found = 0;
    int i = 0;

    vassert(node);
    vassert(addr);
    vassert(srvcId);

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->services); i++){
        svc = (vsrvcInfo*)varray_get(&node->services, i);
        if (vtoken_equal(&svc->id, srvcId) &&
            vsockaddr_equal(&svc->addr, addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        svc = vsrvcInfo_alloc();
        vlog((!svc), elog_vsrvcInfo_alloc);
        ret1E((!svc), vlock_leave(&node->lock));
        vsrvcInfo_init(svc, srvcId, node->nice, addr);
        varray_add_tail(&node->services, svc);
        //node->own_node.weight++;
    }
    vlock_leave(&node->lock);
    return 0;
}

/*
 * the routine to unregister or nulify a service info, which registered before.
 *
 * @node:
 * @srvcId: service hash Id.
 * @addr: address of service to provide.
 *
 */
static
void _vnode_service_unregister(struct vnode* node, vtoken* srvcId, struct sockaddr_in* addr)
{
    vsrvcInfo* svc = NULL;
    int found = 0;
    int i = 0;

    vassert(node);
    vassert(addr);
    vassert(srvcId);

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->services); i++) {
        svc = (vsrvcInfo*)varray_get(&node->services, i);
        if (vtoken_equal(&svc->id, srvcId) &&
            vsockaddr_equal(&svc->addr, addr)) {
            found = 1;
            break;
        }
    }
    if (found) {
        svc = (vsrvcInfo*)varray_del(&node->services, i);
        vsrvcInfo_free(svc);
        //node->own_node.weight--;
    }
    vlock_leave(&node->lock);
    return;
}

/*
 * the routine to update the nice index ( the index of resource avaiblility)
 * of local host. The higher the nice value is, the lower availability for
 * other nodes can provide.
 *
 * @node:
 * @nice : an index of resource availability.
 *
 */
static
void _vnode_service_update(struct vnode* node)
{
    struct vnode_nice* node_nice = &node->node_nice;
    vsrvcInfo* svc = NULL;
    int nice = 0;
    int i = 0;

    vassert(node);

    vlock_enter(&node->lock);
    node->nice = node_nice->ops->get_nice(node_nice);
    for (i = 0; i < varray_size(&node->services); i++) {
        svc = (vsrvcInfo*)varray_get(&node->services, i);
        svc->nice = nice;
    }
    vlock_leave(&node->lock);
    return ;
}

static
void _vnode_service_post(struct vnode* node)
{
    struct vroute* route = node->route;
    vsrvcInfo* svc = NULL;
    int i = 0;
    vassert(node);

    vlock_enter(&node->lock);
    for (i= 0; i < varray_size(&node->services); i++) {
        svc = (vsrvcInfo*)varray_get(&node->services, i);
        route->ops->post_service(route, svc);
    }
    vlock_leave(&node->lock);
    return ;
}

/*
 * the routine to clear all registered service infos.
 *
 * @node:
 */
static
void _vnode_service_clear(struct vnode* node)
{
    vsrvcInfo* svc = NULL;
    vassert(node);

    vlock_enter(&node->lock);
    while(varray_size(&node->services) > 0) {
        svc = (vsrvcInfo*)varray_pop_tail(&node->services);
        vsrvcInfo_free(svc);
    }
    vlock_leave(&node->lock);
    return ;
}

static
void _vnode_service_dump(struct vnode* node)
{
    vsrvcInfo* svc = NULL;
    int i = 0;
    vassert(node);

    vdump(printf("-> LOCAL SERVICES"));
    vlock_enter(&node->lock);
    for (i= 0; i < varray_size(&node->services); i++) {
        svc = (vsrvcInfo*)varray_get(&node->services, i);
        vsrvcInfo_dump(svc);
    }
    vlock_leave(&node->lock);
    vdump(printf("<- LOCAL SERVICES"));
    return ;
}

struct vnode_svc_ops node_svc_ops = {
    .registers  = _vnode_service_register,
    .unregister = _vnode_service_unregister,
    .update     = _vnode_service_update,
    .post       = _vnode_service_post,
    .clear      = _vnode_service_clear,
    .dump       = _vnode_service_dump
};

/*
 * @node:
 * @msger:
 * @ticker:
 * @addr:
 */
int vnode_init(struct vnode* node, struct vconfig* cfg, struct vhost* host, vnodeInfo* node_info)
{
    int ret = 0;

    vassert(node);
    vassert(cfg);
    vassert(host);
    vassert(node_info);

    vlock_init(&node->lock);
    node->mode  = VDHT_OFF;

    vnodeInfo_copy(&node->node_info, node_info);
    node->nice = 5;
    varray_init(&node->services, 4);
   
    node->iport = 13999;
    node->eport = 13999;
    vupnpc_init(&node->upnpc);
    vnode_nice_init(&node->node_nice, cfg);

    node->cfg     = cfg;
    node->ticker  = &host->ticker;
    node->route   = &host->route;
    node->ops     = &node_ops;
    node->svc_ops = &node_svc_ops;

    ret = cfg->ext_ops->get_host_tick_tmo(cfg, &node->tick_interval);
    retE((ret < 0));
    return 0;
}

/*
 * @node:
 */
void vnode_deinit(struct vnode* node)
{
    vassert(node);

    node->svc_ops->clear(node);
    varray_deinit(&node->services);

    node->ops->join(node);
    vupnpc_deinit(&node->upnpc);
    vnode_nice_deinit(&node->node_nice);
    vlock_deinit (&node->lock);

    return ;
}

