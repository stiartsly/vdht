#include "vglobal.h"
#include "vroute.h"

struct vroute_srvc_probe_item {
    vsrvcHash hash;
    vsrvcInfo_number_addr_t  ncb;
    vsrvcInfo_iterate_addr_t icb;
    void* cookie;
    time_t ts;
};

static MEM_AUX_INIT(srvc_probe_item_cache, sizeof(struct vroute_srvc_probe_item), 0);
struct vroute_srvc_probe_item* vroute_srvc_probe_item_alloc(void)
{
    struct vroute_srvc_probe_item* item = NULL;

    item = (struct vroute_srvc_probe_item*)vmem_aux_alloc(&srvc_probe_item_cache);
    vlogEv((!item), elog_vmem_aux_alloc);
    retE_p((!item));

    memset(item, 0, sizeof(*item));
    return item;
}

static
void vroute_srvc_probe_item_free(struct vroute_srvc_probe_item* item)
{
    vassert(item);
    vmem_aux_free(&srvc_probe_item_cache, item);
    return ;
}

static
int vroute_srvc_probe_item_init(struct vroute_srvc_probe_item* item,
            vsrvcHash* hash,
            vsrvcInfo_number_addr_t ncb,
            vsrvcInfo_iterate_addr_t icb,
            void* cookie)
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

static
int _vroute_srvc_probe_helper_add(struct vroute_srvc_probe_helper* probe_helper,
            vsrvcHash* hash,
            vsrvcInfo_number_addr_t ncb,
            vsrvcInfo_iterate_addr_t icb,
            void* cookie)
{
    struct vroute_srvc_probe_item* probe_item = NULL;
    int found = 0;
    int i = 0;

    vassert(probe_helper);
    vassert(hash);
    vassert(ncb);

    for (i = 0; i < varray_size(&probe_helper->items); i++) {
        probe_item = (struct vroute_srvc_probe_item*)varray_get(&probe_helper->items, i);
        if (vtoken_equal(&probe_item->hash, hash) &&
            (ncb == probe_item->ncb) &&
            (icb == probe_item->icb)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        probe_item = vroute_srvc_probe_item_alloc();
        if (!probe_item) {
            return -1;
        }
        vroute_srvc_probe_item_init(probe_item, hash, ncb, icb, cookie);
        varray_add_tail(&probe_helper->items, probe_item);
    }
    return 0;
}

static
int _vroute_srvc_probe_helper_invoke(struct vroute_srvc_probe_helper* probe_helper, vsrvcHash* hash, vsrvcInfo* srvci)
{
    struct vroute_srvc_probe_item* probe_item = NULL;
    int i = 0;
    int j = 0;

    vassert(probe_helper);
    vassert(hash);
    vassert(srvci);

    for (i = 0; i < varray_size(&probe_helper->items);) {
        probe_item = (struct vroute_srvc_probe_item*)varray_get(&probe_helper->items, i);
        if (vtoken_equal(&probe_item->hash, hash)) {
            probe_item->ncb(hash, srvci->naddrs, vsrvcInfo_proto(srvci), probe_item->cookie);
            for (j = 0; j < srvci->naddrs; j++) {
                probe_item->icb(hash, &srvci->addrs[j], (j+1)== srvci->naddrs, probe_item->cookie);
            }
            varray_del(&probe_helper->items, i);
            vroute_srvc_probe_item_free(probe_item);
        } else {
            i++;
        }
    }
    return 0;
}

static
void _vroute_srvc_probe_helper_timed_reap(struct vroute_srvc_probe_helper* probe_helper)
{
    struct vroute_srvc_probe_item* probe_item = NULL;
    time_t now = time(NULL);
    int i = 0;

    for (i = 0; i < varray_size(&probe_helper->items);) {
        probe_item = (struct vroute_srvc_probe_item*)varray_get(&probe_helper->items, i);
        if ((now - probe_item->ts) > probe_helper->max_tmo) {
            varray_del(&probe_helper->items, i);
            vroute_srvc_probe_item_free(probe_item);
        } else {
            i++;
        }
    }
    return;
}

static
void _vroute_srvc_probe_helper_clear(struct vroute_srvc_probe_helper* probe_helper)
{
    struct vroute_srvc_probe_item* probe_item = NULL;
    vassert(probe_helper);

    while (varray_size(&probe_helper->items)) {
        probe_item = (struct vroute_srvc_probe_item*)varray_pop_tail(&probe_helper->items);
        vroute_srvc_probe_item_free(probe_item);
    }
    return ;
}

static
void _vroute_srvc_probe_helper_dump(struct vroute_srvc_probe_helper* probe_helper)
{
    vassert(probe_helper);

    //todo;
    return ;
}

static
struct vroute_srvc_probe_helper_ops route_srvc_probe_helper_ops = {
    .add        = _vroute_srvc_probe_helper_add,
    .invoke     = _vroute_srvc_probe_helper_invoke,
    .timed_reap = _vroute_srvc_probe_helper_timed_reap,
    .clear      = _vroute_srvc_probe_helper_clear,
    .dump       = _vroute_srvc_probe_helper_dump
};

int vroute_srvc_probe_helper_init(struct vroute_srvc_probe_helper* probe_helper)
{
    vassert(probe_helper);

    probe_helper->max_tmo = 5; //max timeout 5s.
    varray_init(&probe_helper->items, 4);
    probe_helper->ops = &route_srvc_probe_helper_ops;
    return 0;
}

void vroute_srvc_probe_helper_deinit(struct vroute_srvc_probe_helper* probe_helper)
{
    vassert(probe_helper);

    probe_helper->ops->clear(probe_helper);
    return;
}

