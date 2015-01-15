#include "vglobal.h"
#include "vnode.h"

/*
 * the routine to get sampling of cpu/mem/io/net used ratio.
 *
 * @node_nice:
 * @return: the niceources(cpu/memory/io/net) used ratio index.
 */
static
int _vnode_nice_get_nice(struct vnode_nice* node_nice)
{
    int nice_val = 0;
    int ret = 0;
    vassert(node_nice);

    ret += vsys_get_cpu_ratio(&node_nice->cpu.ratio);
    ret += vsys_get_mem_ratio(&node_nice->mem.ratio);
    ret += vsys_get_io_ratio (&node_nice->io.ratio);
    ret += vsys_get_net_ratio(&node_nice->net_up.ratio, &node_nice->net_down.ratio);
    if (ret < 0) {
        return node_nice->prev_nice_val;
    }

    nice_val = 0;
    if (node_nice->cpu.ratio >= node_nice->cpu.criteria) {
        nice_val += node_nice->cpu.ratio * node_nice->cpu.factor;
    }
    if (node_nice->mem.ratio >= node_nice->mem.criteria) {
        nice_val += node_nice->mem.ratio * node_nice->mem.factor;
    }
    if (node_nice->io.ratio >= node_nice->io.criteria) {
        nice_val += node_nice->io.ratio  * node_nice->io.factor;
    }
    if (node_nice->net_up.ratio >= node_nice->net_up.criteria) {
        nice_val += node_nice->net_up.ratio  * node_nice->net_up.factor;
    }
    if (node_nice->net_down.ratio >= node_nice->net_down.criteria) {
        nice_val += node_nice->net_down.ratio * node_nice->net_down.factor;
    }
    node_nice->prev_nice_val = nice_val;
    return nice_val;
}

static
struct vnode_nice_ops node_nice_ops = {
    .get_nice = _vnode_nice_get_nice
};

int vnode_nice_init(struct vnode_nice* node_nice, struct vconfig* cfg)
{
    vassert(node_nice);
    vassert(cfg);

    node_nice->cpu.ratio = 5;
    node_nice->mem.ratio = 5;
    node_nice->io.ratio  = 5;
    node_nice->net_up.ratio   = 5;
    node_nice->net_down.ratio = 5;

    node_nice->cpu.criteria = cfg->ops->get_int_val(cfg, "adjust.cpu.criteria");
    if (node_nice->cpu.criteria < 0) {
        node_nice->cpu.criteria = 5;
    }
    node_nice->mem.criteria = cfg->ops->get_int_val(cfg, "adjust.memory.criteria");
    if (node_nice->mem.criteria < 0) {
        node_nice->mem.criteria = 5;
    }
    node_nice->io.criteria  = cfg->ops->get_int_val(cfg, "adjust.io.criteria");
    if (node_nice->io.criteria < 0) {
        node_nice->io.criteria = 5;
    }
    node_nice->net_up.criteria  = cfg->ops->get_int_val(cfg, "adjust.network_upload.criteria");
    if (node_nice->net_up.criteria < 0) {
        node_nice->net_up.criteria = 5;
    }
    node_nice->net_down.criteria  = cfg->ops->get_int_val(cfg, "adjust.network_download.criteria");
    if (node_nice->net_down.criteria < 0) {
        node_nice->net_down.criteria = 5;
    }

    node_nice->cpu.factor = cfg->ops->get_int_val(cfg, "adjust.cpu.factor");
    if (node_nice->cpu.factor < 0) {
        node_nice->cpu.factor = 6;
    }
    node_nice->mem.factor = cfg->ops->get_int_val(cfg, "adjust.memory.factor");
    if (node_nice->mem.factor < 0) {
        node_nice->mem.factor = 5;
    }
    node_nice->io.factor = cfg->ops->get_int_val(cfg, "adjust.io.factor");
    if (node_nice->io.factor < 0) {
        node_nice->io.factor = 3;
    }
    node_nice->net_up.factor = cfg->ops->get_int_val(cfg, "adjust.network_upload.factor");
    if (node_nice->net_up.factor < 0) {
        node_nice->net_up.factor = 6;
    }
    node_nice->net_down.factor = cfg->ops->get_int_val(cfg, "adjust.network_download.factor");
    if (node_nice->net_down.factor < 0) {
        node_nice->net_down.factor = 6;
    }

    node_nice->prev_nice_val = 8;
    node_nice->ops = &node_nice_ops;
    return 0;
}

void vnode_nice_deinit(struct vnode_nice* node_nice)
{
    vassert(node_nice);

    //do nothing;
    return ;
}

