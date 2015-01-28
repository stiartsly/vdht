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
    struct vnode_addr* node_addr = &node->node_addr;
    int ret = 0;
    vassert(node);

    ret = node_addr->ops->setup(node_addr);
    if (ret < 0) {
        vlogI(printf("upnp setup failed"));
    }

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
    struct vnode_addr* node_addr = &node->node_addr;
    vassert(node);

    vlock_enter(&node->lock);
    if ((node->mode == VDHT_OFF) || (node->mode == VDHT_ERR)) {
        vlock_leave(&node->lock);
        return 0;
    }
    node->mode = VDHT_DOWN;
    vlock_leave(&node->lock);

    node_addr->ops->shutdown(node_addr);
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
int _aux_node_get_eaddr(struct sockaddr_in* eaddr, void* cookies)
{
    struct vnode* node = ((void**)cookies)[0];
    vnodeInfo* ni = ((void**)cookies)[1];
    vassert(eaddr);
    vassert(node);

    vlock_enter(&node->lock);
    vsockaddr_copy(&ni->eaddr, eaddr);
    vlock_leave(&node->lock);

    free(cookies);
    return 0;
}

static
int _aux_node_tick_cb(void* cookie)
{
    struct vnode* node   = (struct vnode*)cookie;
    struct vroute* route = node->route;
    struct vnode_addr* node_addr = &node->node_addr;
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
        vnodeInfo* ni = NULL;
        int i = 0;
        for (i = 0; i < varray_size(&node->nodeinfos); i++) {
            ni = (vnodeInfo*)varray_get(&node->nodeinfos, i);
            void* cookies = NULL;
            (void)node_addr->ops->get_uaddr(node_addr, &ni->uaddr);

            cookies = malloc(sizeof(void*) * 2);
            if (!cookie) {
               continue;
            }
            ((void**)cookies)[0] = node;
            ((void**)cookies)[1] = ni;
            (void)node_addr->ops->get_eaddr(node_addr, _aux_node_get_eaddr, cookies);
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
            node->ops->tick(node);
            node->ts = now;
        }
        break;
    }
    case VDHT_DOWN: {
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
void _vnode_dump(struct vnode* node)
{
    int i = 0;
    vassert(node);

    vdump(printf("-> NODE"));
    vlock_enter(&node->lock);
    vdump(printf("state:%s", node_mode_desc[node->mode]));
    vdump(printf("-> NODE_INFO"));
    for (i = 0; i < varray_size(&node->nodeinfos); i++) {
        vnodeInfo* ninfo = (vnodeInfo*)varray_get(&node->nodeinfos, i);
        vnodeInfo_dump(ninfo);
    }
    vdump(printf("<- NODE_INFO"));
    vdump(printf("-> LOCAL SVCS"));
    for (i = 0; i < varray_size(&node->services); i++) {
        vsrvcInfo* svc = (vsrvcInfo*)varray_get(&node->services, i);
        vsrvcInfo_dump(svc);
    }
    vdump(printf("<- LOCAL SVCS"));
    vlock_leave(&node->lock);
    vdump(printf("<- NODE"));
    return ;
}

/*
 * the routine to clear all registered service infos.
 *
 * @node:
 */
static
void _vnode_clear(struct vnode* node)
{
    vassert(node);

    vlock_enter(&node->lock);
    while(varray_size(&node->services) > 0) {
        vsrvcInfo* svc = (vsrvcInfo*)varray_pop_tail(&node->services);
        vsrvcInfo_free(svc);
    }
    vlock_leave(&node->lock);
    return ;
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
    vnodeInfo* ni = (vnodeInfo*)varray_get(&node->nodeinfos, 0);
    if (!vsockaddr_equal(&ni->eaddr, &dest->eaddr)) {
        addr = &dest->eaddr;
    } else if (!vsockaddr_equal(&ni->uaddr, &dest->uaddr)) {
        addr = &dest->uaddr;
    } else {
        addr = &dest->laddr;
    }
    vlock_leave(&node->lock);
    return addr;
}

static
int _vnode_self(struct vnode* node, vnodeInfo* ni)
{
    vassert(node);
    vassert(ni);

    vlock_enter(&node->lock);
    vnodeInfo_copy(ni, (vnodeInfo*)varray_get(&node->nodeinfos, 0));
    vlock_leave(&node->lock);

    return 0;
}

static
int _vnode_is_self(struct vnode* node, struct sockaddr_in* addr)
{
    vnodeInfo* ni = NULL;
    int yes = 0;
    int i = 0;
    vassert(node);
    vassert(addr);

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->nodeinfos); i++) {
        ni = (vnodeInfo*)varray_get(&node->nodeinfos, i);
        yes += vnodeInfo_has_addr(ni, addr);
    }
    vlock_leave(&node->lock);

    return yes;
}

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
int _vnode_reg_service(struct vnode* node, vsrvcId* srvcId, struct sockaddr_in* addr)
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
void _vnode_unreg_service(struct vnode* node, vtoken* srvcId, struct sockaddr_in* addr)
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
int _vnode_renice(struct vnode* node)
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
    return 0;
}

static
void _vnode_tick(struct vnode* node)
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

static
struct vnode_ops node_ops = {
    .start                = _vnode_start,
    .stop                 = _vnode_stop,
    .join                 = _vnode_join,
    .stabilize            = _vnode_stabilize,
    .dump                 = _vnode_dump,
    .clear                = _vnode_clear,
    .reg_service          = _vnode_reg_service,
    .unreg_service        = _vnode_unreg_service,
    .renice               = _vnode_renice,
    .tick                 = _vnode_tick,
    .get_best_usable_addr = _vnode_get_best_usable_addr,
    .self                 = _vnode_self,
    .is_self              = _vnode_is_self
};

static
int _aux_node_get_nodeinfos(struct vconfig* cfg, vnodeId* my_id, struct varray* nodes, vnodeInfo* main_node)
{
    vnodeInfo* ni = NULL;
    struct sockaddr_in laddr;
    vnodeVer node_ver;
    char ip[64];
    int port = 0;
    int ret = 0;

    vassert(cfg);
    vassert(my_id);
    vassert(nodes);

    port = cfg->ext_ops->get_dht_port(cfg);
    vnodeVer_unstrlize(vhost_get_version(), &node_ver);

    memset(ip, 0, 64);
    ret = vhostaddr_get_first(ip, 64);
    retE((ret < 0));
    vsockaddr_convert(ip, port, &laddr);

    ni = vnodeInfo_alloc();
    retE((!ni));
    vnodeInfo_init(ni, my_id, &laddr, &node_ver, 0);
    varray_add_tail(nodes, ni);
    vnodeInfo_copy(main_node, ni);

    while (1) {
        memset(ip, 0, 64);
        ret = vhostaddr_get_next(ip, 64);
        if (ret < 0) {
            break;
        }
        vsockaddr_convert(ip, port, &laddr);

        ni = vnodeInfo_alloc();
        if (!ni) {
            vlog((!ni), elog_vnodeInfo_alloc);
            break;
        }
        vnodeInfo_init(ni, my_id, &laddr, &node_ver, 0);
        varray_add_tail(nodes, ni);
    }
    return 0;
}

/*
 * @node:
 * @msger:
 * @ticker:
 * @addr:
 */
int vnode_init(struct vnode* node, struct vconfig* cfg, struct vhost* host, vnodeId* my_id)
{
    vnodeInfo main_node;
    int ret = 0;

    vassert(node);
    vassert(cfg);
    vassert(host);
    vassert(my_id);

    vlock_init(&node->lock);
    node->mode  = VDHT_OFF;

    node->nice = 5;
    varray_init(&node->nodeinfos, 2);
    varray_init(&node->services, 4);
    ret = _aux_node_get_nodeinfos(cfg, my_id, &node->nodeinfos, &main_node);
    retE((ret < 0));

    vnode_nice_init(&node->node_nice, cfg);
    vnode_addr_init(&node->node_addr, cfg, &host->msger, node, &main_node.laddr);

    node->cfg     = cfg;
    node->ticker  = &host->ticker;
    node->route   = &host->route;
    node->ops     = &node_ops;

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

    node->ops->clear(node);
    varray_deinit(&node->services);

    node->ops->join(node);
    vnode_nice_deinit(&node->node_nice);
    vnode_addr_deinit(&node->node_addr);
    vlock_deinit (&node->lock);

    return ;
}

