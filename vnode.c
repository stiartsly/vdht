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

#define NEED_PORTMAP (16)  // for upnp address.
#define NEED_REFLEX  (17)  // for reflexive address.
#define NEED_PROBE   (18)  // for connectivity probe.

#define need_portmap(mask)  ((mask) &   (1 << NEED_PORTMAP))
#define clear_portmap(mask) ((mask) &= ~(1 << NEED_PORTMAP))
#define set_portmap(mask)   ((mask) |=  (1 << NEED_PORTMAP))

#define need_reflex(mask)   ((mask) &   (1 << NEED_REFLEX ))
#define clear_reflex(mask)  ((mask) &= ~(1 << NEED_REFLEX ))
#define set_reflex(mask)    ((mask) |=  (1 << NEED_REFLEX ))

#define need_probe(mask)    ((mask) &   (1 << NEED_PROBE  ))
#define clear_probe(mask)   ((mask) &= ~(1 << NEED_PROBE  ))
#define set_probe(mask)     ((mask) |=  (1 << NEED_PROBE  ))


/*
 * Before DHT running, we need prepare following things:
 *  1). Acquired host addresses (in @vnode_init);
 *  2). Set up upnp client if possbile;
 *  3). Acquired upnp addresses if possible;
 *  4). Acquired reflexive address if possible;
 *
 * @node: handle to node module.
 */
static
int _vnode_start(struct vnode* node)
{
    struct vupnpc* upnpc = &node->upnpc;
    int ret = 0;
    vassert(node);

    vlock_enter(&node->lock);
    if (node->mode != VDHT_OFF) {
        vlock_leave(&node->lock);
        return 0;
    }
    ret = upnpc->ops->setup(upnpc);
    vlogEv((ret < 0), "upnpc setup error");

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

    upnpc->ops->shutdown(upnpc);
    node->mode = VDHT_DOWN;
    vlock_leave(&node->lock);
    return 0;
}

static
int _vnode_wait_for_stop(struct vnode* node)
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

/*
 * the routine to acquire upnp addresses. If upnpc doesn't work, then
 * skip it.
 * @node: handle to node module.
 */
static
int _aux_node_get_uaddrs(struct vnode* node)
{
    struct vupnpc* upnpc = &node->upnpc;
    vnodeInfo* nodei = node->nodei;
    struct vsockaddr_in uaddr;
    int ret = 0;
    int i = 0;

    vassert(node);
    retS(!upnpc->ops->workable(upnpc));

    for (i = 0; i < nodei->naddrs; i++) {
        if (!need_portmap(nodei->addrs[i].type)) {
            continue;
        }
        ret = upnpc->ops->map(upnpc, &nodei->addrs[i].addr, UPNP_PROTO_UDP, &uaddr.addr);
        clear_portmap(nodei->addrs[i].type);
        if (ret < 0) {
            continue;
        }

        if (vsockaddr_is_private(&uaddr.addr)) {
            uaddr.type = VSOCKADDR_UPNP;
        }else {
            uaddr.type = VSOCKADDR_REFLEXIVE;
        }
        vnodeInfo_add_addr(&nodei, &uaddr.addr, uaddr.type);
    }
    return 0;
}

/*
 * the routine to acquire reflexive addresses.
 * @node:
 */
static
void _aux_node_get_eaddrs(struct vnode* node)
{
    static time_t reflex_ts = 0;
    static int    nreflexs  = 0;
    vnodeInfo* nodei = node->nodei;
    time_t now = time(NULL);
    int i = 0;
    vassert(node);

    /*
     * Reflex address's rule:
     * 1). only 3 times of chance to reflex addresses;
     * 2). the periord between reflexes should more than 5 seconds;
     */
    if ((nreflexs >= 3) || (now - reflex_ts < 5)) {
        return ;
    }
    nreflexs++;
    reflex_ts = time(NULL);

    for (i = 0; i < nodei->naddrs; i++) {
        if (!need_reflex(nodei->addrs[i].type)) {
            continue;
        }
        node->route->ops->reflex(node->route, &nodei->addrs[i].addr);
    }
    return;
}

/*
 * the routine to probe connectivity with each of DHT neighbor nodes.
 * @node:
 */
static
void _aux_node_probe_connectivity(struct vnode* node)
{
    vnodeInfo* nodei = node->nodei;
    int i = 0;
    vassert(node);

    for (i = 0; i < nodei->naddrs; i++) {
        if (!need_probe(nodei->addrs[i].type)) {
            continue;
        }
        node->route->ops->probe_connectivity(node->route, &nodei->addrs[i]);
    }
    return;
}

static
int _aux_node_load_boot_cb(struct sockaddr_in* boot_addr, void* cookie)
{
    struct vnode* node = (struct vnode*)cookie;
    int ret = 0;

    vassert(node);
    vassert(boot_addr);

    if (vnodeInfo_has_addr(node->nodei, boot_addr)) {
        return 0;
    }
    ret = node->route->ops->join_node(node->route, boot_addr);
    retE((ret < 0));
    return 0;
}

static
int _aux_node_tick_cb(void* cookie)
{
    struct vnode* node  = (struct vnode*)cookie;
    struct vconfig* cfg = node->cfg;
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
        (void)node->route->ops->load(node->route);
        (void)cfg->ext_ops->get_boot_nodes(cfg, _aux_node_load_boot_cb, node);
        _aux_node_get_uaddrs(node);
        _aux_node_get_eaddrs(node);

        node->tick_ts = now;
        node->wb_ts   = now;
        node->mode = VDHT_RUN;
        vlogI("DHT start running");
        break;
    }
    case VDHT_RUN: {
        if (node->tick_ts + node->tick_tmo < now) {
            node->ops->renice(node);
            node->ops->tick(node);
            node->tick_ts = now;
        }
        //write back database at certain interval.
        if (node->wb_ts + node->wb_tmo < now) {
            node->route->ops->store(node->route);
            node->wb_ts = now;
        }
        node->route->ops->tick(node->route);
        break;
    }
    case VDHT_DOWN: {
        node->route->ops->store(node->route);
        node->mode = VDHT_OFF;
        vlogI("DHT become offline");
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
    if (node->is_symm_nat) {
        vdump(printf("symmetric NAT"));
    }else {
        vdump(printf("Cone NAT"));
    }
    vdump(printf("tick timeout:%ds", node->tick_tmo));
    vdump(printf("writeback timeout: %ds", node->wb_tmo));
    vdump(printf("-> state:%s", node_mode_desc[node->mode]));
    vdump(printf("-> node infos:"));
    vnodeInfo_dump(node->nodei, printf);
    if (varray_size(&node->services) > 0) {
        vdump(printf("-> list of services:"));
        for (i = 0; i < varray_size(&node->services); i++) {
            vsrvcInfo* srvci = (vsrvcInfo*)varray_get(&node->services, i);
            vsrvcInfo_dump(srvci, printf);
        }
    }
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
        vsrvcInfo* srvci = (vsrvcInfo*)varray_pop_tail(&node->services);
        vsrvcInfo_free(srvci);
    }
    vlock_leave(&node->lock);
    return ;
}

/*
 * the routine to update the nice index ( the index of resource avaiblility)
 * of local host. The higher the nice value is, the lower availability of
 * resource of the node.
 *
 * @node:
 * @nice : an index of resource availability.
 *
 */
static
int _vnode_renice(struct vnode* node)
{
    struct vnode_nice* node_nice = &node->node_nice;
    int i = 0;

    vassert(node);

    vlock_enter(&node->lock);
    node->nice = node_nice->ops->get_nice(node_nice);
    for (i = 0; i < varray_size(&node->services); i++) {
        vsrvcInfo* srvci = (vsrvcInfo*)varray_get(&node->services, i);
        srvci->nice = node->nice;
    }
    vlock_leave(&node->lock);
    return 0;
}

/*
 * the routine to deal some periodical works;
 * @node:
 */
static
void _vnode_tick(struct vnode* node)
{
    struct vroute* route = node->route;
    vsrvcInfo* srvci = NULL;
    int i = 0;
    vassert(node);

    vlock_enter(&node->lock);
    _aux_node_get_eaddrs(node);
    _aux_node_probe_connectivity(node);
    for (i = 0; i < varray_size(&node->services); i++) {
        srvci = (vsrvcInfo*)varray_get(&node->services, i);
        route->ops->air_service(route, srvci);
    }
    vlock_leave(&node->lock);
    return ;
}

/*
 * determine whether DHT node's NAT is symmetric NAT or not.
 * The way to determine whether DHT node's NAT is symmetric NAT:
 *    1. send a @reflex DHT query to DHT node-A to get reflexive address;
 *    2. send a @reflex DHT query to DHT node-B to get reflexive address too;
 *    3. Compare two reflexive addresses. If same, then NAT is Cone NAT;
 *       otherwise, NAT is symmetric NAT.
 *
 * @node:  handle to @vnode module;
 * @laddr: local host address;
 * @eaddr: reflexive address to @laddr;
 */
static
void _aux_node_reflex_NAT(struct vnode* node, struct sockaddr_in* laddr, struct sockaddr_in* eaddr)
{
    struct vnode_nat_probe_data {
        int dirty_mark;
        struct sockaddr_in laddr;
        struct sockaddr_in eaddr;
    };
    static
    struct vnode_nat_probe_data data = {
        .dirty_mark = 0
    };

    if (!vsockaddr_is_public(eaddr)) {
       return;
    }
    if (!data.dirty_mark) {
        vsockaddr_copy(&data.laddr, laddr);
        vsockaddr_copy(&data.eaddr, eaddr);
        data.dirty_mark = 1;
        return;
    }

    if (!vsockaddr_equal(&data.laddr, laddr)) {
        return;
    }
    if (!vsockaddr_equal(&data.eaddr, eaddr)) {
        node->is_symm_nat = 1;
    }
    return;
}
/*
 * the routine to add a reflexive address;
 *
 * @node
 * @laddr: local host address;
 * @eaddr: reflexive address to @laddr;
 */
static
int _vnode_reflex_addr(struct vnode* node, struct sockaddr_in* laddr, struct vsockaddr_in* eaddr)
{
    vnodeInfo* nodei = node->nodei;
    int i = 0;
    vassert(node);
    vassert(eaddr);

    vlock_enter(&node->lock);
    _aux_node_reflex_NAT(node, laddr, &eaddr->addr);
    for (i = 0; i < nodei->naddrs; i++) {
        if (!need_reflex(nodei->addrs[i].type)) {
            continue;
        }
        if (!vsockaddr_equal(laddr, &nodei->addrs[i].addr)) {
            continue;
        }
        vnodeInfo_add_addr(&nodei, &eaddr->addr, eaddr->type);
        clear_reflex(nodei->addrs[i].type);
        break;
    }
    vlock_leave(&node->lock);
    return 0;
}

/*
 * the routine to check whether node contains the address.
 * @node:
 * @addr:
 */
static
int _vnode_has_addr(struct vnode* node, struct sockaddr_in* addr)
{
    int found = 0;

    vassert(node);
    vassert(addr);

    vlock_enter(&node->lock);
    found = vnodeInfo_has_addr(node->nodei, addr);
    vlock_leave(&node->lock);

    return found;
}

static
struct vnode_ops node_ops = {
    .start         = _vnode_start,
    .stop          = _vnode_stop,
    .wait_for_stop = _vnode_wait_for_stop,
    .stabilize     = _vnode_stabilize,
    .dump          = _vnode_dump,
    .clear         = _vnode_clear,
    .renice        = _vnode_renice,
    .tick          = _vnode_tick,
    .reflex_addr   = _vnode_reflex_addr,
    .has_addr      = _vnode_has_addr,
};


/*
 * the routine to post a service entry, which is in scope of DHT's PC/deivce.
 * Simply, the posted service entry is just cached in @vnode module.
 * and later will be periodically broadcast to all neighbor DHT nodes.
 *
 * @node:
 * @hash:  service hash ID.
 * @addr:  entry address of serivce.
 * @proto: protocol to service, only support "TCP" or "UDP";
 */
static
int _vnode_srvc_post(struct vnode* node, vsrvcHash* hash, struct vsockaddr_in* addr, int proto)
{
    vsrvcInfo* srvci = NULL;
    int found = 0;
    int ret = 0;
    int i = 0;

    vassert(node);
    vassert(hash);
    vassert(addr);
    vassert((proto > VPROTO_RES0) && (proto < VPROTO_RES1));

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->services); i++){
        srvci = (vsrvcInfo*)varray_get(&node->services, i);
        if (vsrvcHash_equal(&srvci->hash, hash)) {
            found = 1;
            if (proto != srvci->proto) {
                ret = -1;
            } else {
                vsrvcInfo_add_addr(&srvci, &addr->addr, addr->type);
                varray_set(&node->services, i, srvci);
            }
            break;
        }
    }
    if ((ret >= 0) && (!found)) {
        srvci = vsrvcInfo_alloc();
        vlogEv((!srvci), elog_vsrvcInfo_alloc);
        ret1E((!srvci), vlock_leave(&node->lock));

        vsrvcInfo_init(srvci, hash, &node->nodei->id, node->nice, proto);
        vsrvcInfo_add_addr(&srvci, &addr->addr, addr->type);
        varray_add_tail(&node->services, srvci);
        node->nodei->weight++;
    }
    vlock_leave(&node->lock);
    return ret;
}

/*
 * the routine to unpost an address of service entry.
 * Simply, just clear the service entry if found. Therefore, this service entry
 * will not be broadcast at next later times; *
 *
 * @node:
 * @hash: service hash ID;
 * @addr: address entry of that service.
 */
static
int _vnode_srvc_unpost(struct vnode* node, vsrvcHash* hash, struct sockaddr_in* addr)
{
    vsrvcInfo* srvci = NULL;
    int found = 0;
    int i = 0;

    vassert(node);
    vassert(addr);
    vassert(hash);

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->services); i++) {
        srvci = (vsrvcInfo*)varray_get(&node->services, i);
        if (vsrvcHash_equal(&srvci->hash, hash)) {
            vsrvcInfo_del_addr(srvci, addr);
            found = 1;
            break;
        }
    }
    if ((found) && (vsrvcInfo_is_empty(srvci))) {
        srvci = (vsrvcInfo*)varray_del(&node->services, i);
        vsrvcInfo_free(srvci);
        node->nodei->weight--;
    }
    vlock_leave(&node->lock);
    return 0;
}

/*
 * the routine to unpost a whole service entry.
 * @node:
 * @hash: service hash ID.
 */
static
int _vnode_srvc_unpost_ext(struct vnode* node, vsrvcHash* hash)
{
    vsrvcInfo* srvci = NULL;
    int found = 0;
    int i = 0;

    vassert(node);
    vassert(hash);

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->services); i++) {
        srvci = (vsrvcInfo*)varray_get(&node->services, i);
        if (vsrvcHash_equal(&srvci->hash, hash)) {
            found = 1;
            break;
        }
    }
    if (found) {
        srvci = (vsrvcInfo*)varray_del(&node->services, i);
        vsrvcInfo_free(srvci);
        node->nodei->weight--;
    }
    vlock_leave(&node->lock);
    return 0;
}

/*
 * the routine to find a service entry by given hash ID.
 * Notice: we only find service entries in
 *  1). PC/device-scope of current DHT node(cached in @vnode module);
 *  2). service route space of current DHT node.
 * and return the result immediately in synchronization.
 *
 * @node:
 * @hash: service hash ID.
 * @ncb: callback to show number of address entries or no service entry found.
 * @icb: callback to show each of address entries;
 * @cookie: user data;
 */
static
int _vnode_srvc_find(struct vnode* node, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vroute* route = node->route;
    DECL_VSRVC_RELAX(srvci);
    vsrvcInfo* item = NULL;
    int found = 0;
    int ret = 0;
    int i = 0;

    vassert(node);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    vlock_enter(&node->lock);
    for (i = 0; i < varray_size(&node->services); i++) {
        item = (vsrvcInfo*)varray_get(&node->services, i);
        if (vsrvcHash_equal(&item->hash, hash)) {
            found = 1;
            break;
        }
    }
    if (found) {
        vsrvcInfo_copy(&srvci, item);
    }
    vlock_leave(&node->lock);

    if (found) {
        ncb(hash, srvci->naddrs, srvci->proto, cookie);
        for (i = 0; i < srvci->naddrs; i++) {
            icb(hash, &srvci->addrs[i].addr, srvci->addrs[i].type, (i+1) == srvci->naddrs, cookie);
        }
        return 0;
    }

    ret = route->ops->find_service(route, hash, ncb, icb, cookie);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to probe a service entry by given hash ID.
 * Notice: we only probe service entries in
 *  2). service route space of neighbor DHT nodes.
 * and result result in asynchrnonization by invoking callbacks.
 *
 * @node:
 * @hash: service hash ID.
 * @ncb: callback to show number of address entries or no service entry found.
 * @icb: callback to show each of address entries;
 * @cookie: user data;
 */
static
int _vnode_srvc_probe(struct vnode* node, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vroute* route = node->route;
    int ret = 0;

    vassert(node);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    ret = route->ops->probe_service(route, hash, ncb, icb, cookie);
    retE((ret < 0));
    return 0;
}

static
struct vnode_srvc_ops node_srvc_ops = {
    .post       = _vnode_srvc_post,
    .unpost     = _vnode_srvc_unpost,
    .unpost_ext = _vnode_srvc_unpost_ext,
    .find       = _vnode_srvc_find,
    .probe      = _vnode_srvc_probe
};

static
void _aux_node_add_addr_cb(struct sockaddr_in* addr, void* cookie)
{
    vnodeInfo* nodei = (vnodeInfo*)cookie;
    uint32_t   type  = 0;

    vassert(addr);
    vassert(nodei);

    /*
     *  If local host address is private address, then:
     *   1). need to use upnpc to port mapping.
     *   2). need to get correspond reflexive address.
     */
    if (vsockaddr_is_private(addr)) {
        type = VSOCKADDR_LOCAL;
        set_portmap(type);
        set_reflex(type);
    } else {
        type = VSOCKADDR_REFLEXIVE;
    }
    set_probe(type);
    vnodeInfo_add_addr(&nodei, addr, type);
    return ;
}

static
int _aux_node_get_nodeinfo(vnodeInfo* nodei, vnodeId* myid, struct vconfig* cfg, void (*cb)(struct sockaddr_in*, void*))
{
    struct sockaddr_in addr;
    vnodeVer ver;
    char ip[64] = {0};
    uint16_t port = 0;
    int num = 0;
    int ret = 0;

    vassert(nodei);
    vassert(cfg);
    vassert(cb);

    vnodeVer_unstrlize(vhost_get_version(), &ver);
    vnodeInfo_relax_init(nodei, myid, &ver, 0);

    ret = cfg->ext_ops->get_dht_address(cfg, &addr);
    retE((ret < 0));
    if ((uint32_t)addr.sin_addr.s_addr) {
        cb(&addr, nodei);
        return 0;
    }
    port = ntohs(addr.sin_port);
    ret = vhostaddr_get_first(ip, 64);
    retE((ret < 0));
    vsockaddr_convert(ip, port, &addr);
    cb(&addr, nodei);
    num++;

    while(num < 3) {
        memset(ip, 0, 64);
        ret = vhostaddr_get_next(ip, 64);
        if (ret <= 0) {
            break;
        }
        vsockaddr_convert(ip, port, &addr);
        cb(&addr, nodei);
        num++;
    }
    return 0;
}

/*
 * @node:
 * @msger:
 * @ticker:
 * @addr:
 */
int vnode_init(struct vnode* node, struct vconfig* cfg, struct vhost* host, vnodeId* myid)
{
    int ret = 0;

    vassert(node);
    vassert(cfg);
    vassert(host);
    vassert(myid);

    node->nodei = (vnodeInfo*)&node->nodei_relax;
    memset(node->nodei, 0, sizeof(vnodeInfo_relax));
    node->nodei->capc = VNODEINFO_MAX_ADDRS;

    ret = _aux_node_get_nodeinfo(node->nodei, myid, cfg, _aux_node_add_addr_cb);
    retE((ret < 0));

    vlock_init(&node->lock);
    node->mode  = VDHT_OFF;

    node->is_symm_nat = 0;
    node->nice = 5;
    varray_init(&node->services, 4);
    vnode_nice_init(&node->node_nice, cfg);
    vupnpc_init(&node->upnpc);

    node->cfg     = cfg;
    node->ticker  = &host->ticker;
    node->route   = &host->route;
    node->ops     = &node_ops;
    node->srvc_ops= &node_srvc_ops;
    node->tick_tmo= 30; //30seconds;
    node->wb_tmo  = cfg->ext_ops->get_host_wb_tmo(cfg);

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

    node->ops->wait_for_stop(node);
    vupnpc_deinit(&node->upnpc);
    vnode_nice_deinit(&node->node_nice);
    vlock_deinit (&node->lock);

    return ;
}

