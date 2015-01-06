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
    int ret = 0;
    vassert(res);
    vassert(cfg);

    res->cpu.ratio  = 0;
    res->mem.ratio  = 0;
    res->io.ratio   = 0;
    res->up.ratio   = 0;
    res->down.ratio = 0;

    ret += cfg->inst_ops->get_cpu_criteria(cfg, &res->cpu.criteria);
    ret += cfg->inst_ops->get_mem_criteria(cfg, &res->mem.criteria);
    ret += cfg->inst_ops->get_io_criteria (cfg, &res->io.criteria);
    ret += cfg->inst_ops->get_up_criteria (cfg, &res->up.criteria);
    ret += cfg->inst_ops->get_down_criteria(cfg, &res->down.criteria);
    ret += cfg->inst_ops->get_cpu_factor(cfg, &res->cpu.factor);
    ret += cfg->inst_ops->get_mem_factor(cfg, &res->mem.factor);
    ret += cfg->inst_ops->get_io_factor (cfg, &res->io.factor);
    ret += cfg->inst_ops->get_up_factor (cfg, &res->up.factor);
    ret += cfg->inst_ops->get_down_factor(cfg, &res->down.factor);
    retE((ret < 0));

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
    struct vroute*  route = kicker->route;
    int nice = 0;
    int ret  = 0;

    vassert(kicker);
    vassert(route);

    ret = res->ops->get_nice(res, &nice);
    retE((ret < 0));

    ret = route->ops->kick_nice(route, nice);
    retE((ret < 0));
    return 0;
}

static
int _vkicker_kick_upnp_addr(struct vkicker* kicker)
{
    struct vupnpc* upnpc = &kicker->upnpc;
    struct vroute* route = kicker->route;
    struct sockaddr_in upnp_addr;
    int ret = 0;

    vassert(kicker);
    vassert(route);

    ret = upnpc->ops->setup(upnpc);
    retE((ret < 0));
    ret = upnpc->ops->map(upnpc, 15999, 17999, 1, &upnp_addr);
    retE((ret < 0));

    //todo;
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

int vkicker_init(struct vkicker* kicker, struct vroute* route, struct vconfig* cfg)
{
    int ret = 0;
    vassert(kicker);
    vassert(route);
    vassert(cfg);

    ret = vresource_init(&kicker->res, cfg);
    retE((ret < 0));
    ret = vupnpc_init(&kicker->upnpc);
    retE((ret < 0));

    kicker->route = route;
    kicker->cfg   = cfg;
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

