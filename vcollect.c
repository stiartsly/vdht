#include "vglobal.h"
#include "vcollect.h"

static
int _vcollect_collect_data(struct vcollect* collect)
{
    int ret = 0;
    vassert(collect);

    ret = vsys_get_cpu_ratio(&collect->cpu_ratio);
    retE((ret < 0));
    ret = vsys_get_mem_ratio(&collect->mem_ratio);
    retE((ret < 0));
    ret = vsys_get_io_ratio (&collect->io_ratio);
    retE((ret < 0));
    ret = vsys_get_net_ratio(&collect->up_ratio, &collect->down_ratio);
    retE((ret < 0));
    return 0;
}

static
int _vcollect_get_index(struct vcollect* collect, int* index)
{
    vassert(collect);
    vassert(index);

    *index = 0;
    if (collect->cpu_ratio >= collect->cpu_criteria) {
        *index += collect->cpu_ratio * collect->cpu_factor;
    }
    if (collect->mem_ratio >= collect->mem_criteria) {
        *index += collect->mem_ratio * collect->mem_factor;
    }
    if (collect->io_ratio >= collect->io_criteria) {
        *index += collect->io_ratio  * collect->io_factor;
    }
    if (collect->up_ratio >= collect->up_criteria) {
        *index += collect->up_ratio  * collect->up_factor;
    }
    if (collect->down_ratio >= collect->down_criteria) {
        *index += collect->down_ratio * collect->down_factor;
    }

    return 0;
}

static
struct vcollect_ops collect_ops = {
    .collect_data = _vcollect_collect_data,
    .get_index    = _vcollect_get_index
};

int vcollect_init(struct vcollect* collect, struct vconfig* cfg)
{
    int ret = 0;

    vassert(collect);
    vassert(cfg);

    collect->cpu_ratio  = 0;
    collect->mem_ratio  = 0;
    collect->io_ratio   = 0;
    collect->up_ratio   = 0;
    collect->down_ratio = 0;

    cfg->ops->get_int_ext(cfg, "collect.cpu_criteria", &collect->cpu_criteria, DEF_COLLECT_CPU_CRITERIA);
    cfg->ops->get_int_ext(cfg, "collect.cpu_factor",   &collect->cpu_factor,   DEF_COLLECT_CPU_FACTOR);
    cfg->ops->get_int_ext(cfg, "collect.mem_criteria", &collect->mem_criteria, DEF_COLLECT_MEM_CRITERIA);
    cfg->ops->get_int_ext(cfg, "collect.mem_factor",   &collect->mem_factor,   DEF_COLLECT_MEM_FACTOR);
    cfg->ops->get_int_ext(cfg, "collect.io_criteria",  &collect->io_criteria,  DEF_COLLECT_IO_CRITERIA);
    cfg->ops->get_int_ext(cfg, "collect.io_factor",    &collect->io_factor,    DEF_COLLECT_IO_FACTOR);
    cfg->ops->get_int_ext(cfg, "collect.up_criteria",  &collect->up_criteria,  DEF_COLLECT_UP_CRITERIA);
    cfg->ops->get_int_ext(cfg, "collect.up_factor",    &collect->up_factor,    DEF_COLLECT_UP_FACTOR);
    cfg->ops->get_int_ext(cfg, "collect.down_critieria", &collect->down_criteria, DEF_COLLECT_DOWN_CRITERIA);
    cfg->ops->get_int_ext(cfg, "collect.down_factor",  &collect->down_criteria, DEF_COLLECT_DOWN_FACTOR);

    collect->ops = &collect_ops;
    return 0;
}

void vcollect_deinit(struct vcollect* collect)
{
    vassert(collect);
    return ;
}

