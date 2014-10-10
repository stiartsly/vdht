#include "vglobal.h"
#include "vticker.h"

#define to_vtick_cb(node) vlist_entry(node, struct vtick_cb, list)
/*
 * for vtick_cb
 */
static MEM_AUX_INIT(tcb_cache, sizeof(struct vtick_cb), 8);
struct vtick_cb* vtick_cb_alloc(void)
{
    struct vtick_cb* tcb = NULL;
    tcb = vmem_aux_alloc(&tcb_cache);
    vlog_cond((!tcb), elog_vmem_aux_alloc);
    ret1E_p((!tcb));
    return tcb;
}

void vtick_cb_free(struct vtick_cb* tcb)
{
    vassert(tcb);
    vmem_aux_free(&tcb_cache, tcb);
    return ;
}

void vtick_cb_init(struct vtick_cb* tcb, vtick_t cb, void* cookie)
{
    vassert(tcb);
    vassert(cb);
    vassert(cookie);

    vlist_init(&tcb->list);
    tcb->cb = cb;
    tcb->cookie = cookie;
    return ;
}

/*
 * for ticker
 */
static
int _vticker_add_cb(struct vticker* ticker, vtick_t cb, void* cookie)
{
    struct vtick_cb* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(ticker);
    vassert(cb);
    vassert(cookie);

    item = vtick_cb_alloc();
    vlog_cond((!item), elog_vtick_cb_alloc);
    retE((!item));
    vtick_cb_init(item, cb, cookie);

    vlock_enter(&ticker->lock);
    vlist_add_tail(&ticker->cbs, &item->list);
    vlock_leave(&ticker->lock);
    return 0;
}

/*
 * @ticker:
 * @tomo: timeout
 */
static
int _vticker_start(struct vticker* ticker, int tmo)
{
    int ret = 0;
    vassert(ticker);

    ret = vtimer_start(&ticker->timer, tmo);
    vlog_cond((ret < 0), elog_vtimer_start);
    retE((ret < 0));
    return 0;
}

/*
 * @ticker:
 * @tmo: timeout;
 */
static
int _vticker_restart(struct vticker* ticker, int tmo)
{
    int ret = 0;
    vassert(ticker);

    ret = vtimer_restart(&ticker->timer, tmo);
    vlog_cond((ret < 0), elog_vtimer_restart);
    retE((ret < 0));
    return 0;
}

/*
 * @ticker:
 */
static
int _vticker_stop(struct vticker* ticker)
{
    int ret = 0;
    vassert(ticker);
    ret = vtimer_stop(&ticker->timer);
    vlog_cond((ret < 0), elog_vtimer_stop);
    retE((ret < 0));
    return 0;
}

/*
 * @ticker:
 */
static
int _vticker_clear(struct vticker* ticker)
{
    struct vlist* node = NULL;
    vassert(ticker);

    vlock_enter(&ticker->lock);
    while(!vlist_is_empty(&ticker->cbs)) {
        node = vlist_pop_head(&ticker->cbs);
        vtick_cb_free(to_vtick_cb(node));
    }
    vlock_leave(&ticker->lock);
    return 0;
}

static
struct vticker_ops ticker_ops = {
    .add_cb  = _vticker_add_cb,
    .start   = _vticker_start,
    .restart = _vticker_restart,
    .stop    = _vticker_stop,
    .clear   = _vticker_clear
};

static
int _aux_timer_cb(void* cookie)
{
    struct vticker* ticker = (struct vticker*)cookie;
    struct vtick_cb* item = NULL;
    struct vlist*    node = NULL;
    vassert(ticker);

    vlock_enter(&ticker->lock);
    __vlist_for_each(node, &ticker->cbs) {
        item = to_vtick_cb(node);
        item->cb(item->cookie);
    }
    vlock_leave(&ticker->lock);
    return 0;
}

int vticker_init(struct vticker* ticker)
{
    int ret = 0;
    vassert(ticker);

    memset(ticker, 0, sizeof(*ticker));
    vlist_init(&ticker->cbs);
    vlock_init(&ticker->lock);
    ticker->ops = &ticker_ops;

    ret = vtimer_init(&ticker->timer, _aux_timer_cb, ticker);
    vlog_cond((ret < 0), elog_vtimer_init);
    retE((ret < 0));
    return 0;
}

void vticker_deinit(struct vticker* ticker)
{
    vassert(ticker);

    vtimer_deinit(&ticker->timer);
    ticker->ops->clear(ticker);
    vlock_deinit(&ticker->lock);
    return ;
}

