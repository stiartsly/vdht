#include "vglobal.h"
#include "vroute.h"

/*
 * service node structure and it follows it's correspondent help APIs.
 */
struct vservice {
    vsrvcId*   srvcId;
    vsrvcInfo* srvc;
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

    if (item->srvc) {
        vsrvcInfo_free(item->srvc);
    }
    vmem_aux_free(&service_cache, item);
    return ;
}

static
int vservice_init(struct vservice* item, vsrvcInfo* srvc, time_t ts)
{
    vsrvcInfo* srvc_old = item->srvc;
    vsrvcInfo* srvc_dup = NULL;
    vassert(item);
    vassert(srvc);

    srvc_dup = vsrvcInfo_dup(srvc);
    retE((!srvc_dup));

    item->srvcId = &srvc_dup->id;
    item->srvc   = srvc_dup;
    item->rcv_ts = ts;

    if (srvc_old) {
        vsrvcInfo_free(srvc_old);
    }
    return 0;
}

static
void vservice_dump(struct vservice* item)
{
    vassert(item);

    vsrvcInfo_dump(item->srvc);
    printf("timestamp[rcv]: %s",  ctime(&item->rcv_ts));
    return;
}

static
int _aux_srvc_add_service_cb(void* item, void* cookie)
{
    struct vservice* svc_item = (struct vservice*)item;
    varg_decl(cookie, 0, struct vservice**, to);
    varg_decl(cookie, 1, vsrvcInfo*, srvc);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int*, max_period);
    varg_decl(cookie, 4, int*, found);

    if (vtoken_equal(svc_item->srvcId, &srvc->id)){
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
    struct vservice* srvc_item = (struct vservice*)item;
    varg_decl(cookie, 0, struct vservice**, to);
    varg_decl(cookie, 1, vtoken*, srvcId);
    varg_decl(cookie, 2, int*, min_nice);

    if (vtoken_equal(srvc_item->srvcId, srvcId) &&
        (srvc_item->srvc->nice < *min_nice)) {
        *to = srvc_item;
        *min_nice = srvc_item->srvc->nice;
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
int _vroute_srvc_add_service(struct vroute_srvc_space* space, vsrvcInfo* srvc)
{
    struct varray* srvcs = NULL;
    struct vservice*  to = NULL;
    time_t now = time(NULL);
    int max_period = 0;
    int found = 0;

    vassert(space);
    vassert(srvc);

    srvcs = &space->bucket[vsrvcId_bucket(&srvc->id)].srvcs;
    {
        void* argv[] = {
            &to,
            srvc,
            &now,
            &max_period,
            &found
        };
        varray_iterate(srvcs, _aux_srvc_add_service_cb, argv);
        if (found) {
            vservice_init(to, srvc, now);
        } else if (to && varray_size(srvcs) >= space->bucket_sz) {
            vservice_init(to, srvc, now);
        } else if (varray_size(srvcs) < space->bucket_sz) {
            to = vservice_alloc();
            retE((!to));
            vservice_init(to, srvc, now);
            varray_add_tail(srvcs, to);
        } else {
            //bucket is full, discard it (worst one).
        }
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
int _vroute_srvc_get_service(struct vroute_srvc_space* space, vsrvcId* srvcId, vsrvcInfo** srvc)
{
    struct varray* srvcs = NULL;
    struct vservice*  to = NULL;
    int min_nice = 100;
    int found = 0;

    vassert(space);
    vassert(srvcId);
    vassert(srvc);


    srvcs = &space->bucket[vsrvcId_bucket(srvcId)].srvcs;
    {
        void* argv[] = {
            &to,
            srvcId,
            &min_nice
        };
        varray_iterate(srvcs, _aux_srvc_get_service_cb, argv);
        if (to) {
            *srvc = vsrvcInfo_dup(to->srvc);
            if (*srvc) {
                found = 1;
            }
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
    int titled = 0;
    int i = 0;
    int j = 0;

    vassert(space);

    for (i = 0; i < NBUCKETS; i++) {
        svcs = &space->bucket[i].srvcs;
        for (j = 0; j < varray_size(svcs); j++) {
            if (!titled) {
                vdump(printf("-> list of services in service routing space:"));
                titled = 1;
            }
            printf("{ ");
            vservice_dump((struct vservice*)varray_get(svcs, j));
            printf(" }\n");
        }
    }
    return;
}

static
struct vroute_srvc_space_ops route_srvc_space_ops = {
    .add_service = _vroute_srvc_add_service,
    .get_service = _vroute_srvc_get_service,
    .clear       = _vroute_srvc_clear,
    .dump        = _vroute_srvc_dump
};

int vroute_srvc_space_init(struct vroute_srvc_space* space, struct vconfig* cfg)
{
    int i = 0;
    vassert(space);
    vassert(cfg);

    for (i = 0; i < NBUCKETS; i++) {
        varray_init(&space->bucket[i].srvcs, 8);
    }
    space->bucket_sz = cfg->ext_ops->get_route_bucket_sz(cfg);
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

