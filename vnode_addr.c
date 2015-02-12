#include "vglobal.h"
#include "vnode.h"

static
int _vnode_addr_setup(struct vnode_addr* node_addr)
{
    struct vupnpc* upnpc = &node_addr->upnpc;
    int ret = 0;
    vassert(node_addr);

    ret = upnpc->ops->setup(upnpc);
    retE((ret < 0));
    return 0;
}

static
void _vnode_addr_shutdown(struct vnode_addr* node_addr)
{
    struct vupnpc* upnpc = &node_addr->upnpc;
    vassert(node_addr);

    upnpc->ops->shutdown(upnpc);
    return ;
}

static
int _vnode_addr_get_uaddr(struct vnode_addr* node_addr, struct sockaddr_in* laddr, struct sockaddr_in* uaddr)
{
    struct vupnpc* upnpc = &node_addr->upnpc;
    int ret = 0;

    vassert(node_addr);
    vassert(uaddr);

    ret = upnpc->ops->map(upnpc, laddr, UPNP_PROTO_UDP, uaddr);
    retE((ret < 0));
    return 0;
}

static
int _vnode_addr_get_eaddr(struct vnode_addr* node_addr, get_ext_addr_t cb, void* cookie)
{
    struct vroute* route = node_addr->node->route;
    struct vstun* stun   = &node_addr->stun;
    struct sockaddr_in stuns_addr;
    vsrvcId srvId;
    int ret = 0;

    vassert(node_addr);
    vassert(cb);

    ret = vhashgen_get_stun_srvcId(&srvId);
    retE((ret < 0));
    ret = route->ops->get_service(route, &srvId, &stuns_addr);
    if (ret <= 0) {
        return -1;
    }
    ret = stun->ops->get_ext_addr(stun, cb, cookie, &stuns_addr);
    retE((ret < 0));
    return 0;
}

static
int _vnode_addr_get_raddr(struct vnode_addr* node_addr, struct sockaddr_in* raddr)
{
    vassert(node_addr);
    vassert(raddr);

    //todo;
    return 0;
}

static
int _vnode_addr_pub_stuns(struct vnode_addr* node_addr, struct sockaddr_in* eaddr)
{
    struct vstun* stun = &node_addr->stun;
    int ret = 0;
    vassert(node_addr);
    vassert(eaddr);

    if (node_addr->pubed) {
        return 0;
    }

    ret = stun->ops->reg_service(stun, eaddr);
    retE((ret < 0));
    node_addr->pubed = 1;
    return 0;
}

static
struct vnode_addr_ops node_addr_ops = {
    .setup     = _vnode_addr_setup,
    .shutdown  = _vnode_addr_shutdown,
    .get_uaddr = _vnode_addr_get_uaddr,
    .get_eaddr = _vnode_addr_get_eaddr,
    .get_raddr = _vnode_addr_get_raddr,
    .pub_stuns = _vnode_addr_pub_stuns
};

int vnode_addr_init(struct vnode_addr* node_addr, struct vmsger* msger, struct vnode* node)
{
    vassert(node_addr);
    vassert(msger);
    vassert(node);

    node_addr->node  = node;
    node_addr->pubed = 0;

    vupnpc_init(&node_addr->upnpc);
    vstun_init (&node_addr->stun, msger, node);
    node_addr->ops = &node_addr_ops;
    return 0;
}

void vnode_addr_deinit(struct vnode_addr* node_addr)
{
    vassert(node_addr);

    vstun_deinit (&node_addr->stun);
    vupnpc_deinit(&node_addr->upnpc);
    return ;
}

