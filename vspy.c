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

    ret += vsys_get_cpu_ratio(&spy->cpu.ratio);
    ret += vsys_get_mem_ratio(&spy->mem.ratio);
    ret += vsys_get_io_ratio (&spy->io.ratio);
    ret += vsys_get_net_ratio(&spy->up.ratio, &spy->down.ratio);
    retE((ret < 0));

    *nice = 0;
    if (spy->cpu.ratio >= spy->cpu.criteria) {
        *nice += spy->cpu.ratio * spy->cpu.factor;
    }
    if (spy->mem.ratio >= spy->mem.criteria) {
        *nice += spy->mem.ratio * spy->mem.factor;
    }
    if (spy->io.ratio >= spy->io.criteria) {
        *nice += spy->io.ratio  * spy->io.factor;
    }
    if (spy->up.ratio >= spy->up.criteria) {
        *nice += spy->up.ratio  * spy->up.factor;
    }
    if (spy->down.ratio >= spy->down.criteria) {
        *nice += spy->down.ratio * spy->down.factor;
    }
    return 0;
}

static
int _vspy_get_nat_type(struct vspy* spy, int* nat_type)
{
    vassert(spy);
    vassert(nat_type);

    //todo;
    return 0;
}

static
struct vspy_ops spy_ops = {
    .get_nice     = _vspy_get_nice,
    .get_nat_type = _vspy_get_nat_type
};

int vspy_init(struct vspy* spy, struct vconfig* cfg)
{
    int ret = 0;
    vassert(spy);
    vassert(cfg);

    spy->cpu.ratio  = 0;
    spy->mem.ratio  = 0;
    spy->io.ratio   = 0;
    spy->up.ratio   = 0;
    spy->down.ratio = 0;

    ret += cfg->inst_ops->get_cpu_criteria(cfg, &spy->cpu.criteria);
    ret += cfg->inst_ops->get_mem_criteria(cfg, &spy->mem.criteria);
    ret += cfg->inst_ops->get_io_criteria (cfg, &spy->io.criteria);
    ret += cfg->inst_ops->get_up_criteria (cfg, &spy->up.criteria);
    ret += cfg->inst_ops->get_down_criteria(cfg, &spy->down.criteria);
    ret += cfg->inst_ops->get_cpu_factor(cfg, &spy->cpu.factor);
    ret += cfg->inst_ops->get_mem_factor(cfg, &spy->mem.factor);
    ret += cfg->inst_ops->get_io_factor (cfg, &spy->io.factor);
    ret += cfg->inst_ops->get_up_factor (cfg, &spy->up.factor);
    ret += cfg->inst_ops->get_down_factor(cfg, &spy->down.factor);
    retE((ret < 0));

    spy->ops = &spy_ops;
    return 0;
}

void vspy_deinit(struct vspy* spy)
{
    vassert(spy);
    return ;
}

