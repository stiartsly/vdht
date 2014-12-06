#include "vglobal.h"
#include "vperf.h"

static
int _vperf_start(struct vperf* perf)
{
    vassert(perf);
    //todo;
    return 0;
}

static
int _vperf_stop(struct vperf* perf)
{
    vassert(perf);
    //todo;
    return 0;
}

static
int _vperf_enter(struct vperf* perf)
{
    vassert(perf);
    //todo;
    return 0;
}

static
int _vperf_leave(struct vperf* perf)
{
    vassert(perf);
    //todo;
    return 0;
}

static
struct vperf_ops perf_ops = {
    .start = _vperf_start,
    .stop  = _vperf_stop,
    .enter = _vperf_enter,
    .leave = _vperf_leave
};

int vperf_init(struct vperf* perf, struct vconfig* cfg, struct vticker* ticker)
{
    vassert(perf);
    vassert(cfg);
    vassert(ticker);

    perf->ops = &perf_ops;
    //todo;
    return 0;
}

void vperf_deinit(struct vperf* perf)
{
    vassert(perf);
    //todo;
    return ;
}

