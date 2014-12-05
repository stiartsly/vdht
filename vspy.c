#include "vglobal.h"
#include "vspy.h"

/*
 * the routine to get sampling of cpu/mem/io/net used ratio.
 *
 * @spy:
 * @nice: the resources(cpu/memory/io/net) used ratio index.
 */
static
int _vspy_get_nice(struct vspy* spy, int* nice)
{
    int ret = 0;
    vassert(spy);
    vassert(nice);

    ret += vsys_get_cpu_ratio(&spy->cpu_ratio);
    ret += vsys_get_mem_ratio(&spy->mem_ratio);
    ret += vsys_get_io_ratio (&spy->io_ratio);
    ret += vsys_get_net_ratio(&spy->up_ratio, &spy->down_ratio);
    retE((ret < 0));

    *nice = 0;
    if (spy->cpu_ratio >= spy->cpu_criteria) {
        *nice += spy->cpu_ratio * spy->cpu_factor;
    }
    if (spy->mem_ratio >= spy->mem_criteria) {
        *nice += spy->mem_ratio * spy->mem_factor;
    }
    if (spy->io_ratio >= spy->io_criteria) {
        *nice += spy->io_ratio  * spy->io_factor;
    }
    if (spy->up_ratio >= spy->up_criteria) {
        *nice += spy->up_ratio  * spy->up_factor;
    }
    if (spy->down_ratio >= spy->down_criteria) {
        *nice += spy->down_ratio * spy->down_factor;
    }
    return 0;
}

static
struct vspy_ops spy_ops = {
    .get_nice = _vspy_get_nice
};

int vspy_init(struct vspy* spy, struct vconfig* cfg)
{
    int ret = 0;
    vassert(spy);
    vassert(cfg);

    spy->cpu_ratio  = 0;
    spy->mem_ratio  = 0;
    spy->io_ratio   = 0;
    spy->up_ratio   = 0;
    spy->down_ratio = 0;

    ret += cfg->inst_ops->get_cpu_criteria(cfg, &spy->cpu_criteria);
    ret += cfg->inst_ops->get_mem_criteria(cfg, &spy->mem_criteria);
    ret += cfg->inst_ops->get_io_criteria (cfg, &spy->io_criteria);
    ret += cfg->inst_ops->get_up_criteria (cfg, &spy->up_criteria);
    ret += cfg->inst_ops->get_down_criteria(cfg, &spy->down_criteria);
    ret += cfg->inst_ops->get_cpu_factor(cfg, &spy->cpu_factor);
    ret += cfg->inst_ops->get_mem_factor(cfg, &spy->mem_factor);
    ret += cfg->inst_ops->get_io_factor (cfg, &spy->io_factor);
    ret += cfg->inst_ops->get_up_factor (cfg, &spy->up_factor);
    ret += cfg->inst_ops->get_down_factor(cfg, &spy->down_factor);
    retE((ret < 0));

    spy->ops = &spy_ops;
    return 0;
}

void vspy_deinit(struct vspy* spy)
{
    vassert(spy);
    return ;
}

