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
    struct vnode_res_status_p {
        int* ratio;
        int* criteria;
        int* factor;
    };
    struct vnode_res_status_p node_res_status_p[] = {
        {&node_nice->cpu.ratio,      &node_nice->cpu.criteria,      &node_nice->cpu.factor      },
        {&node_nice->mem.ratio,      &node_nice->mem.criteria,      &node_nice->mem.factor      },
        {&node_nice->io.ratio,       &node_nice->io.criteria,       &node_nice->io.factor       },
        {&node_nice->net_up.ratio,   &node_nice->net_up.criteria,   &node_nice->net_up.factor   },
        {&node_nice->net_down.ratio, &node_nice->net_down.criteria, &node_nice->net_down.factor },
        {NULL, NULL, NULL}
    };
    struct vnode_res_status_p* res_status_p = node_res_status_p;
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
    for (; res_status_p->ratio; res_status_p++) {
        if (*res_status_p->ratio >= *res_status_p->criteria) {
            nice_val += *res_status_p->ratio * (*res_status_p->factor);
        }
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
    struct vnice_criteria_desc {
        const char* key;
        int* criteria;
        int  def_val;
    };
    struct vnice_factor_desc {
        const char* key;
        int* factor;
        int  def_val;
    };

    struct vnice_criteria_desc nice_criteria_desc[] = {
        {"adjust.cpu.criteria",              &node_nice->cpu.criteria,      5 },
        {"adjust.memory.criteria",           &node_nice->mem.criteria,      5 },
        {"adjust.io.criteria",               &node_nice->io.criteria,       5 },
        {"adjust.network_upload.criteria",   &node_nice->net_up.criteria,   5 },
        {"adjust.network_download.criteria", &node_nice->net_down.criteria, 5 },
        {NULL, NULL, 0}
    };
    struct vnice_criteria_desc* criteria_desc = nice_criteria_desc;
    struct vnice_factor_desc nice_factor_desc[] = {
        {"adjust.cpu.factor",                &node_nice->cpu.factor,        6 },
        {"adjust.memory.factor",             &node_nice->mem.factor,        5 },
        {"adjust.io.factor",                 &node_nice->io.factor,         3 },
        {"adjust.network_upload.factor",     &node_nice->net_up.factor,     6 },
        {"adjust.network_download.factor",   &node_nice->net_down.factor,   6 },
        {NULL, NULL, 0}
    };
    struct vnice_factor_desc* factor_desc = nice_factor_desc;

    vassert(node_nice);
    vassert(cfg);

    node_nice->cpu.ratio = 5;
    node_nice->mem.ratio = 5;
    node_nice->io.ratio  = 5;
    node_nice->net_up.ratio   = 5;
    node_nice->net_down.ratio = 5;

    for (; criteria_desc->key; criteria_desc++) {
        int criteria = cfg->ops->get_int_val(cfg, criteria_desc->key);
        if (criteria < 0) {
            criteria = criteria_desc->def_val;
        }
        *criteria_desc->criteria = criteria;
    }
    for (; factor_desc->key; factor_desc++) {
        int factor = cfg->ops->get_int_val(cfg, factor_desc->key);
        if (factor < 0) {
            factor = factor_desc->def_val;
        }
        *factor_desc->factor = factor;
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

