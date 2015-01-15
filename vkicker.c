#include "vglobal.h"
#include "vkicker.h"

/*
 * the routine to get sampling of cpu/mem/io/net used ratio.
 *
 * @res:
 * @nice: the resources(cpu/memory/io/net) used ratio index.
 */
static
int _vresource_get_nice(struct vresource* res, int* nice)
{
    int ret = 0;
    vassert(res);
    vassert(nice);

    ret += vsys_get_cpu_ratio(&res->cpu.ratio);
    ret += vsys_get_mem_ratio(&res->mem.ratio);
    ret += vsys_get_io_ratio (&res->io.ratio);
    ret += vsys_get_net_ratio(&res->up.ratio, &res->down.ratio);
    retE((ret < 0));

    *nice = 0;
    if (res->cpu.ratio >= res->cpu.criteria) {
        *nice += res->cpu.ratio * res->cpu.factor;
    }
    if (res->mem.ratio >= res->mem.criteria) {
        *nice += res->mem.ratio * res->mem.factor;
    }
    if (res->io.ratio >= res->io.criteria) {
        *nice += res->io.ratio  * res->io.factor;
    }
    if (res->up.ratio >= res->up.criteria) {
        *nice += res->up.ratio  * res->up.factor;
    }
    if (res->down.ratio >= res->down.criteria) {
        *nice += res->down.ratio * res->down.factor;
    }
    return 0;
}

struct vresource_ops resource_ops = {
    .get_nice = _vresource_get_nice
};

int vresource_init(struct vresource* res, struct vconfig* cfg)
{
    vassert(res);
    vassert(cfg);

    res->cpu.ratio  = 0;
    res->mem.ratio  = 0;
    res->io.ratio   = 0;
    res->up.ratio   = 0;
    res->down.ratio = 0;

    res->cpu.criteria = cfg->ops->get_int_val(cfg, "adjust.cpu.criteria");
    if (res->cpu.criteria < 0) {
        res->cpu.criteria = 5;
    }
    res->mem.criteria = cfg->ops->get_int_val(cfg, "adjust.memory.criteria");
    if (res->mem.criteria < 0) {
        res->mem.criteria = 5;
    }
    res->io.criteria  = cfg->ops->get_int_val(cfg, "adjust.io.criteria");
    if (res->io.criteria < 0) {
        res->io.criteria = 5;
    }
    res->up.criteria  = cfg->ops->get_int_val(cfg, "adjust.network_upload.criteria");
    if (res->up.criteria < 0) {
        res->up.criteria = 5;
    }
    res->down.criteria  = cfg->ops->get_int_val(cfg, "adjust.network_download.criteria");
    if (res->down.criteria < 0) {
        res->down.criteria = 5;
    }

    res->cpu.factor = cfg->ops->get_int_val(cfg, "adjust.cpu.factor");
    if (res->cpu.factor < 0) {
        res->cpu.factor = 6;
    }
    res->mem.factor = cfg->ops->get_int_val(cfg, "adjust.memory.factor");
    if (res->mem.factor < 0) {
        res->mem.factor = 5;
    }
    res->io.factor = cfg->ops->get_int_val(cfg, "adjust.io.factor");
    if (res->io.factor < 0) {
        res->io.factor = 3;
    }
    res->up.factor = cfg->ops->get_int_val(cfg, "adjust.network_upload.factor");
    if (res->up.factor < 0) {
        res->up.factor = 6;
    }
    res->down.factor = cfg->ops->get_int_val(cfg, "adjust.network_download.factor");
    if (res->down.factor < 0) {
        res->down.factor = 6;
    }
    res->ops = &resource_ops;
    return 0;
}

void vresource_void(struct vresource* res)
{
    vassert(res);

    //todo;
    return ;
}

static
int _vkicker_kick_nice(struct vkicker* kicker)
{
    struct vresource* res = &kicker->res;
    struct vnode* node = kicker->node;
    int nice = 0;
    int ret  = 0;

    vassert(kicker);

    ret = res->ops->get_nice(res, &nice);
    retE((ret < 0));

    node->svc_ops->update(node, nice);
    return 0;
}

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
    .kick_nice       = _vkicker_kick_nice,
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

    ret = vresource_init(&kicker->res, cfg);
    retE((ret < 0));
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

