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

    //todo: for vstunc
    return 0;
}

static
void _vnode_addr_shutdown(struct vnode_addr* node_addr)
{
    struct vupnpc* upnpc = &node_addr->upnpc;
    vassert(node_addr);

    upnpc->ops->shutdown(upnpc);

    //todo: for vstunc;
    return ;
}

static
int _vnode_addr_get_uaddr(struct vnode_addr* node_addr, struct sockaddr_in* uaddr)
{
    struct vupnpc* upnpc = &node_addr->upnpc;
    int eport = node_addr->iport;
    int ret = 0;

    vassert(node_addr);
    vassert(uaddr);

    ret = upnpc->ops->map(upnpc, node_addr->iport, eport, UPNP_PROTO_UDP, uaddr);
    retE((ret < 0));

    node_addr->eport = eport;
    return 0;
}

static
int _vnode_addr_get_eaddr(struct vnode_addr* node_addr, struct sockaddr_in* eaddr)
{
    vassert(node_addr);
    vassert(eaddr);

    //todo;
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
struct vnode_addr_ops node_addr_ops = {
    .setup     = _vnode_addr_setup,
    .shutdown  = _vnode_addr_shutdown,
    .get_uaddr = _vnode_addr_get_uaddr,
    .get_eaddr = _vnode_addr_get_eaddr,
    .get_raddr = _vnode_addr_get_raddr
};

int vnode_addr_init(struct vnode_addr* node_addr, struct vconfig* cfg)
{
    int ret = 0;
    vassert(node_addr);
    vassert(cfg);

    ret = cfg->ext_ops->get_dht_port(cfg, &node_addr->iport);
    retE((ret < 0));
    node_addr->eport = 0;

    vupnpc_init(&node_addr->upnpc);
    node_addr->ops = &node_addr_ops;
    return 0;
}

void vnode_addr_deinit(struct vnode_addr* node_addr)
{
    vassert(node_addr);

    vupnpc_deinit(&node_addr->upnpc);
    return ;
}

