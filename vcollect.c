#include "vglobal.h"
#include "vcollect.h"

static
int _vcollect_res_used_ratio(struct vcollect* collect)
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
int _vcollect_get_nice(struct vcollect* collect, int* index)
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
    .collect_used_ratio = _vcollect_res_used_ratio,
    .get_nice           = _vcollect_get_nice
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

    ret += cfg->inst_ops->get_cpu_criteria(cfg, &collect->cpu_criteria);
    ret += cfg->inst_ops->get_mem_criteria(cfg, &collect->mem_criteria);
    ret += cfg->inst_ops->get_io_criteria (cfg, &collect->io_criteria);
    ret += cfg->inst_ops->get_up_criteria (cfg, &collect->up_criteria);
    ret += cfg->inst_ops->get_down_criteria(cfg, &collect->down_criteria);
    ret += cfg->inst_ops->get_cpu_factor(cfg, &collect->cpu_factor);
    ret += cfg->inst_ops->get_mem_factor(cfg, &collect->mem_factor);
    ret += cfg->inst_ops->get_io_factor (cfg, &collect->io_factor);
    ret += cfg->inst_ops->get_up_factor (cfg, &collect->up_factor);
    ret += cfg->inst_ops->get_down_factor(cfg, &collect->down_factor);
    retE((ret < 0));

    collect->ops = &collect_ops;
    return 0;
}

void vcollect_deinit(struct vcollect* collect)
{
    vassert(collect);
    return ;
}

