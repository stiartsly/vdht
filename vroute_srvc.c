#include "vglobal.h"
#include "vroute.h"

/*
 * service node structure and it follows it's correspondent help APIs.
 */
struct vservice {
    vsrvcInfo svc;
    time_t rcv_ts;
};

static MEM_AUX_INIT(service_cache, sizeof(struct vservice), 8);
static
struct vservice* vservice_alloc(void)
{
    struct vservice* item = NULL;

    item = (struct vservice*)vmem_aux_alloc(&service_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    memset(item, 0, sizeof(*item));
    return item;
}

static
void vservice_free(struct vservice* item)
{
    vassert(item);
    vmem_aux_free(&service_cache, item);
    return ;
}

static
void vservice_init(struct vservice* item, vsrvcInfo* svci, time_t ts)
{
    vassert(item);
    vassert(svci);

    vsrvcInfo_copy(&item->svc, svci);
    item->rcv_ts = ts;
    return ;
}

static
void vservice_dump(struct vservice* item)
{
    vassert(item);
    vdump(printf("-> SERVICE"));
    vsrvcInfo_dump(&item->svc);
    vdump(printf("timestamp[rcv]: %s",  ctime(&item->rcv_ts)));
    vdump(printf("<- SERVICE"));
    return;
}

static
int _aux_srvc_add_service_cb(void* item, void* cookie)
{
    struct vservice* svc_item = (struct vservice*)item;
    varg_decl(cookie, 0, struct vservice**, to);
    varg_decl(cookie, 1, vsrvcInfo*, svc);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int*, max_period);
    varg_decl(cookie, 4, int*, found);

    if (vsrvcInfo_equal(&svc_item->svc, svc)) {
        *to = svc_item;
        *found = 1;
        return 1;
    }
    if ((int)(*now - svc_item->rcv_ts) > *max_period) {
        *to = svc_item;
        *max_period = *now - svc_item->rcv_ts;
    }
    return 0;
}

static
int _aux_srvc_get_service_cb(void* item, void* cookie)
{
    struct vservice* svc_item = (struct vservice*)item;
    varg_decl(cookie, 0, struct vservice**, to);
    varg_decl(cookie, 1, vtoken*, svc_hash);
    varg_decl(cookie, 2, int*, min_nice);

    if (vtoken_equal(&svc_item->svc.id, svc_hash) &&
        (svc_item->svc.nice < *min_nice)) {
        *to = svc_item;
        *min_nice = svc_item->svc.nice;
    }
    return 0;
}

/*
 * the routine to add service (from other nodes) into service routing table.
 *
 * @space: service routing table space.
 * @svci : service infos
 *
 */
static
int _vroute_srvc_add_service_node(struct vroute_srvc_space* space, vsrvcInfo* svc)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int max_period = 0;
    int found = 0;

    vassert(space);
    vassert(svc);

    svcs = &space->bucket[vsrvcId_bucket(&svc->id)].srvcs;
    {
        void* argv[] = {
            &to,
            svc,
            &now,
            &max_period,
            &found
        };
        varray_iterate(svcs, _aux_srvc_add_service_cb, argv);
        if (found) {
            to->rcv_ts = now;
        } else if (to && varray_size(svcs) >= space->bucket_sz) {
            vservice_init(to, svc, now);
        } else {
            to = vservice_alloc();
            retE((!to));
            vservice_init(to, svc, now);
            varray_add_tail(svcs, to);
        };
    }
    return 0;
}

/*
 * the routine to get service info from service routing table space if local host
 * require some kind of system service.
 *
 * @space: service routing table space;
 * @svc_hash:
 * @svci : service infos
 */
static
int _vroute_srvc_get_service_node(struct vroute_srvc_space* space, vtoken* svc_hash, vsrvcInfo* svc)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    int min_nice = 100;
    int found = 0;

    vassert(space);
    vassert(svc);
    vassert(svc_hash);

    svcs = &space->bucket[vsrvcId_bucket(svc_hash)].srvcs;
    {
        void* argv[] = {
            &to,
            svc_hash,
            &min_nice
        };
        varray_iterate(svcs, _aux_srvc_get_service_cb, argv);
        if (to) {
            vsrvcInfo_copy(svc, &to->svc);
            found = 1;
        }
    }
    return found;
}

/*
 * the routine to clear all service nodes in service routing table.
 *
 * @sapce:
 */
static
void _vroute_srvc_clear(struct vroute_srvc_space* space)
{
    struct varray* svcs = NULL;
    int i = 0;
    vassert(space);

    for (i = 0; i < NBUCKETS; i++) {
        svcs = &space->bucket[i].srvcs;
        while(varray_size(svcs) > 0) {
            vservice_free((struct vservice*)varray_del(svcs, 0));
        }
    }
    return;
}

/*
 * the routine to dump all service nodes in service routing table
 *
 * @space:
 */
static
void _vroute_srvc_dump(struct vroute_srvc_space* space)
{
    struct varray* svcs = NULL;
    int i = 0;
    int j = 0;

    vassert(space);

    vdump(printf("-> SRVC ROUTING SPACE"));
    for (i = 0; i < NBUCKETS; i++) {
        svcs = &space->bucket[i].srvcs;
        for (j = 0; j < varray_size(svcs); j++) {
            vservice_dump((struct vservice*)varray_get(svcs, j));
        }
    }
    vdump(printf("<- SRVC ROUTING SPACE"));
    return;
}

static
struct vroute_srvc_space_ops route_srvc_space_ops = {
    .add_srvc_node = _vroute_srvc_add_service_node,
    .get_srvc_node = _vroute_srvc_get_service_node,
    .clear         = _vroute_srvc_clear,
    .dump          = _vroute_srvc_dump
};

int vroute_srvc_space_init(struct vroute_srvc_space* space, struct vconfig* cfg)
{
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(cfg);

    for (i = 0; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].srvcs, 8);
    }
    ret = cfg->inst_ops->get_route_bucket_sz(cfg, &space->bucket_sz);
    retE((ret < 0));
    space->ops = &route_srvc_space_ops;

    return 0;
}

void vroute_srvc_space_deinit(struct vroute_srvc_space* space)
{
    int i = 0;

    space->ops->clear(space);
    for (i = 0; i < NBUCKETS; i++) {
        varray_deinit(&space->bucket[i].srvcs);
    }
    return ;
}

