#include "vglobal.h"
#include "vnode.h"

#define CPU_CRITERIA      ((int)8)
#define CPU_FACTOR        ((int)7)
#define CPU_RATIO         ((int)2)
#define MEM_CRITERIA      ((int)8)
#define MEM_FACTOR        ((int)6)
#define MEM_RATIO         ((int)2)
#define IO_CRITERIA       ((int)4)
#define IO_FACTOR         ((int)3)
#define IO_RATIO          ((int)2)
#define NET_UP_CRITERIA   ((int)8)
#define NET_UP_FACTOR     ((int)3)
#define NET_UP_RATIO      ((int)2)
#define NET_DOWN_CRITERIA ((int)5)
#define NET_DOWN_FACTOR   ((int)2)
#define NET_DOWN_RATIO    ((int)2)

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
        if (*res_status_p->ratio < *res_status_p->criteria) {
            nice_val += *res_status_p->ratio * (*res_status_p->factor);
        } else {
            nice_val += 10 * (*res_status_p->factor);
        }
    }
    nice_val /= 10;
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

    node_nice->cpu.criteria      = CPU_CRITERIA;
    node_nice->cpu.factor        = CPU_FACTOR;
    node_nice->cpu.ratio         = CPU_RATIO;
    node_nice->mem.criteria      = MEM_CRITERIA;
    node_nice->mem.factor        = MEM_FACTOR;
    node_nice->mem.ratio         = MEM_RATIO;
    node_nice->io.criteria       = IO_CRITERIA;
    node_nice->io.factor         = IO_FACTOR;
    node_nice->io.ratio          = IO_RATIO;
    node_nice->net_up.criteria   = NET_UP_CRITERIA;
    node_nice->net_up.factor     = NET_UP_FACTOR;
    node_nice->net_up.ratio      = NET_UP_RATIO;
    node_nice->net_down.criteria = NET_DOWN_CRITERIA;
    node_nice->net_down.factor   = NET_DOWN_FACTOR;
    node_nice->net_down.ratio    = NET_DOWN_RATIO;

    node_nice->prev_nice_val = 2;
    node_nice->ops = &node_nice_ops;
    return 0;
}

void vnode_nice_deinit(struct vnode_nice* node_nice)
{
    vassert(node_nice);
    //do nothing;
    return ;
}

