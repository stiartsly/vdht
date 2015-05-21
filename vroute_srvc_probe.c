#include "vglobal.h"
#include "vroute.h"

struct vsrvc_probe {
    vsrvcHash hash;
    vsrvcInfo_number_addr_t  ncb;
    vsrvcInfo_iterate_addr_t icb;
    void* cookie;
    time_t ts;
};

static MEM_AUX_INIT(srvc_probe_cache, sizeof(struct vsrvc_probe), 0);
struct vsrvc_probe* vsrvc_probe_alloc(void)
{
    struct vsrvc_probe* item = NULL;

    item = (struct vsrvc_probe*)vmem_aux_alloc(&srvc_probe_cache);
    vlogEv((!item), elog_vmem_aux_alloc);
    retE_p((!item));

    memset(item, 0, sizeof(*item));
    return item;
}

static
void vsrvc_probe_free(struct vsrvc_probe* item)
{
    vassert(item);
    vmem_aux_free(&srvc_probe_cache, item);
    return ;
}

static
int vsrvc_probe_init(struct vsrvc_probe* item, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    vassert(item);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    vtoken_copy(&item->hash, hash);
    item->ncb = ncb;
    item->icb = icb;
    item->cookie = cookie;
    item->ts  = time(NULL);

    return 0;
}

/*
 *
 * @space;
 * @hash:
 * @ncb:
 * @icb:
 * @cookie
 */
static
int _vroute_srvc_probe_space_add_cb(struct vroute_srvc_probe_space* space, vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vsrvc_probe* item = NULL;
    int found = 0;
    int i = 0;

    vassert(space);
    vassert(hash);
    vassert(ncb);
    vassert(icb);

    for (i = 0; i < varray_size(&space->items); i++) {
        item = (struct vsrvc_probe*)varray_get(&space->items, i);
        if (vtoken_equal(&item->hash, hash) &&
            (ncb == item->ncb) &&
            (icb == item->icb)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        item = vsrvc_probe_alloc();
        retE((!item));
        vsrvc_probe_init(item, hash, ncb, icb, cookie);
        varray_add_tail(&space->items, item);
    }
    return 0;
}

/*
 *
 * @space:
 * @hash:
 * @srvci:
 */
static
int _vroute_srvc_probe_space_invoke(struct vroute_srvc_probe_space* space, vsrvcHash* hash, vsrvcInfo* srvci)
{
    struct vsrvc_probe* item = NULL;
    int i = 0;
    int j = 0;

    vassert(space);
    vassert(hash);
    vassert(srvci);

    for (i = 0; i < varray_size(&space->items);) {
        item = (struct vsrvc_probe*)varray_get(&space->items, i);
        if (vtoken_equal(&item->hash, hash)) {
            item->ncb(hash, srvci->naddrs, vsrvcInfo_proto(srvci), item->cookie);
            for (j = 0; j < srvci->naddrs; j++) {
                item->icb(hash, &srvci->addrs[j].addr, srvci->addrs[j].type, (j+1)== srvci->naddrs, item->cookie);
            }
            varray_del(&space->items, i);
            vsrvc_probe_free(item);
        } else {
            i++;
        }
    }
    return 0;
}

/*
 * the routine to reap the service-probe record if timeout happens
 *
 * @space:
 */
static
void _vroute_srvc_probe_space_timed_reap(struct vroute_srvc_probe_space* space)
{
    struct vsrvc_probe* item = NULL;
    time_t now = time(NULL);
    int i = 0;

    for (i = 0; i < varray_size(&space->items);) {
        item = (struct vsrvc_probe*)varray_get(&space->items, i);
        if ((now - item->ts) > space->max_tmo) {
            varray_del(&space->items, i);
            vsrvc_probe_free(item);
        } else {
            i++;
        }
    }
    return;
}

/*
 * the routine to clean all service-probe records.
 *
 * @space:
 */
static
void _vroute_srvc_probe_space_clear(struct vroute_srvc_probe_space* space)
{
    struct vsrvc_probe* item = NULL;
    vassert(space);

    while (varray_size(&space->items) > 0) {
        item = (struct vsrvc_probe*)varray_pop_tail(&space->items);
        vsrvc_probe_free(item);
    }
    return ;
}

/*
 * the routine to dump all service-probe records.
 *
 * @space:
 */
static
void _vroute_srvc_probe_space_dump(struct vroute_srvc_probe_space* space)
{
    vassert(space);

    //todo;
    return ;
}

static
struct vroute_srvc_probe_space_ops route_srvc_probe_space_ops = {
    .add_cb     = _vroute_srvc_probe_space_add_cb,
    .invoke     = _vroute_srvc_probe_space_invoke,
    .timed_reap = _vroute_srvc_probe_space_timed_reap,
    .clear      = _vroute_srvc_probe_space_clear,
    .dump       = _vroute_srvc_probe_space_dump
};

int vroute_srvc_probe_space_init(struct vroute_srvc_probe_space* space)
{
    vassert(space);

    space->max_tmo = 5; //max timeout 5s.
    varray_init(&space->items, 4);
    space->ops = &route_srvc_probe_space_ops;
    return 0;
}

void vroute_srvc_probe_space_deinit(struct vroute_srvc_probe_space* space)
{
    vassert(space);
    space->ops->clear(space);
    return;
}

