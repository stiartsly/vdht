#include "vglobal.h"
#include "vroute.h"

struct vservice_desc {
    int   what;
    char* desc;
};

static struct vservice_desc service_desc[] = {
    { PLUGIN_RELAY,  "relay"           },
    { PLUGIN_STUN,   "stun"            },
    { PLUGIN_VPN,    "vpn"             },
    { PLUGIN_DDNS,   "ddns"            },
    { PLUGIN_MROUTE, "multiple routes" },
    { PLUGIN_DHASH,  "data hash"       },
    { PLUGIN_APP,    "app"             },
    { 0, 0}
};

char* vroute_srvc_get_desc(int what)
{
    struct vservice_desc* desc = service_desc;
    for (; desc->desc; ) {
        if (desc->what == what) {
            break;
        }
        desc++;
    }
    if (!desc->desc) {
        return "unkown service";
    }
    return desc->desc;
}

/*
 * service node structure and it follows it's correspondent help APIs.
 */
struct vservice {
    struct sockaddr_in addr;
    int what;
    int nice;
    uint32_t flags;
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
void vservice_init(
        struct vservice* item,
        int what,
        int nice,
        struct sockaddr_in* addr,
        time_t ts,
        uint32_t flags)
{
    vassert(item);
    vassert(addr);

    vsockaddr_copy(&item->addr, addr);
    item->what   = what;
    item->nice   = nice;
    item->rcv_ts = ts;
    item->flags  = flags;
    return ;
}

static
void vservice_dump(struct vservice* item)
{
    vassert(item);
    vdump(printf("-> SERVICE"));
    vdump(printf("what:%s", vroute_srvc_get_desc(item->what)));
    vdump(printf("nice:%d", item->nice));
    vdump(printf("timestamp[rcv]: %s",  ctime(&item->rcv_ts)));
    vsockaddr_dump(&item->addr);
    vdump(printf("<- SERVICE"));
    return;
}

static
int _aux_srvc_add_service_cb(void* item, void* cookie)
{
    struct vservice* svc = (struct vservice*)item;
    varg_decl(cookie, 0, vsrvcInfo*, svci);
    varg_decl(cookie, 1, struct vservice**, to);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int32_t*, min_tdff);
    varg_decl(cookie, 4, int*, found);

    if ((svc->what == svci->what)
        && vsockaddr_equal(&svc->addr, &svci->addr)) {
        *to = svc;
        *found = 1;
        return 1;
    }
    if ((*now - svc->rcv_ts) < *min_tdff) {
        *to = svc;
        *min_tdff = *now - svc->rcv_ts;
    }
    return 0;
}

static
int _aux_srvc_get_service_cb(void* item, void* cookie)
{
    struct vservice* svc = (struct vservice*)item;
    varg_decl(cookie, 1, struct vservice**, to);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int32_t*, min_tdff);

    if ((*now - svc->rcv_ts) < *min_tdff) {
        *to = svc;
        *min_tdff = *now - svc->rcv_ts;
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
int _vroute_srvc_add_service_node(struct vroute_srvc_space* space, vsrvcInfo* svci)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int32_t min_tdff = (int32_t)0x7fffffff;
    int found = 0;
    int updt = 0;
    int ret  = -1;

    vassert(space);
    vassert(svci);
    retE((svci->what < 0));
    retE((svci->what >= PLUGIN_BUTT));

    svcs = &space->bucket[svci->what].srvcs;
    {
        void* argv[] = {
            svci,
            &to,
            &now,
            &min_tdff,
            &found
        };
        varray_iterate(svcs, _aux_srvc_add_service_cb, argv);
        if (found) {
            to->rcv_ts = now;
            updt = 1;
        } else if (to) {
            vservice_init(to, svci->what, svci->nice, &svci->addr, now, 0);
            updt = 1;
        } else {
            to = vservice_alloc();
            retE((!to));
            vservice_init(to, svci->what, svci->nice, &svci->addr, now, 0);
            varray_add_tail(svcs, to);
            updt = 1;
        };
    }
    if (updt) {
        space->bucket[svci->what].ts = now;
        ret = 0;
    }
    return ret;
}

/*
 * the routine to get service info from service routing table space if local host
 * require some kind of system service.
 *
 * @space: service routing table space;
 * @svci : service infos
 */
static
int _vroute_srvc_get_serivce_node(struct vroute_srvc_space* space, int what, vsrvcInfo* svci)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int32_t min_tdff = (int32_t)0x7fffffff;
    int ret = -1;

    vassert(space);
    vassert(svci);

    svcs = &space->bucket[what].srvcs;
    {
        void* argv[] = {
            &to,
            &now,
            &min_tdff
        };
        varray_iterate(svcs, _aux_srvc_get_service_cb, argv);
        if (to) {
            vsrvcInfo_init(svci, what, to->nice, &to->addr);
            ret = 0;
        }
    }
    return ret;
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

    for (; i < PLUGIN_BUTT; i++) {
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
    for (i = 0; i < PLUGIN_BUTT; i++) {
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
    .get_srvc_node = _vroute_srvc_get_serivce_node,
    .clear         = _vroute_srvc_clear,
    .dump          = _vroute_srvc_dump
};

int vroute_srvc_space_init(struct vroute_srvc_space* space, struct vconfig* cfg)
{
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(cfg);

    for (; i < PLUGIN_BUTT; i++) {
        varray_init(&space->bucket[i].srvcs, 8);
        space->bucket[i].ts = 0;
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
    for (; i < PLUGIN_BUTT; i++) {
        varray_deinit(&space->bucket[i].srvcs);
    }
    return ;
}

