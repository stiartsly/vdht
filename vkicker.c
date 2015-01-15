#include "vglobal.h"
#include "vkicker.h"

static
int _vkicker_kick_upnp_addr(struct vkicker* kicker)
{
    struct vupnpc*  upnpc = &kicker->upnpc;
    //struct vroute*  route = kicker->route;
    struct vconfig* cfg   = kicker->cfg;
    struct sockaddr_in upnp_addr;
    int iport = 0;
    int eport = 0;
    int ret = 0;
    int i = 0;

    vassert(kicker);

    ret = cfg->ext_ops->get_dht_port(cfg, &iport);
    retE((ret < 0));

    ret = upnpc->ops->setup(upnpc);
    retE((ret < 0));

    for (i = 0; i < 5; i++) {
        eport = iport + i;
        ret = upnpc->ops->map(upnpc, (uint16_t)iport, (uint16_t)eport , 1, &upnp_addr);
        if (ret >= 0) {
            break;
        }
    }
  //  ret = route->ops->kick_upnp_addr(route, &upnp_addr);
  //  retE((ret < 0));
    vsockaddr_dump(&upnp_addr);
    return 0;
}

static
int _vkicker_kick_external_addr(struct vkicker* kicker)
{
    vassert(kicker);
    //todo;
    return 0;
}

static
int _vkicker_kick_relayed_addr(struct vkicker* kicker)
{
    vassert(kicker);
    //todo;
    return 0;
}

static
int _vkicker_kick_nat_type(struct vkicker* kicker)
{
    vassert(kicker);
    //todo;
    return 0;
}

static
struct vkicker_ops kicker_ops = {
    .kick_upnp_addr  = _vkicker_kick_upnp_addr,
    .kick_ext_addr   = _vkicker_kick_external_addr,
    .kick_relay_addr = _vkicker_kick_relayed_addr,
    .kick_nat_type   = _vkicker_kick_nat_type
};

int vkicker_init(struct vkicker* kicker, struct vnode* node, struct vconfig* cfg)
{
    int ret = 0;
    vassert(kicker);
    vassert(node);
    vassert(cfg);

    ret = vupnpc_init(&kicker->upnpc);
    retE((ret < 0));

    kicker->node = node;
    kicker->cfg  = cfg;
    //todo;
    kicker->ops = &kicker_ops;
    return 0;
}

void vkicker_deinit(struct vkicker* kicker)
{
    vassert(kicker);
    vupnpc_deinit(&kicker->upnpc);
    return ;
}

