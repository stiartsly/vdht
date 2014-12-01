#include "vglobal.h"
#include "vroute.h"


struct vplugin_desc {
    int   what;
    char* desc;
};

static
struct vplugin_desc plugin_desc[] = {
    { PLUGIN_RELAY,  "relay"           },
    { PLUGIN_STUN,   "stun"            },
    { PLUGIN_VPN,    "vpn"             },
    { PLUGIN_DDNS,   "ddns"            },
    { PLUGIN_MROUTE, "multiple routes" },
    { PLUGIN_DHASH,  "data hash"       },
    { PLUGIN_APP,    "app"             },
    { PLUGIN_BUTT, 0}
};

char* vpluger_get_desc(int what)
{
    struct vplugin_desc* desc = plugin_desc;

    for (; desc->desc; ) {
        if (desc->what == what) {
            break;
        }
        desc++;
    }
    if (!desc->desc) {
        return "unkown plugin";
    }
    return desc->desc;
}

/*
 * for plug_item
 */
struct vservice {
    struct sockaddr_in addr;
    int what;
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
void vservice_init(struct vservice* item, int what, struct sockaddr_in* addr, time_t ts, uint32_t flags)
{
    vassert(item);
    vassert(addr);

    vsockaddr_copy(&item->addr, addr);
    item->what   = what;
    item->rcv_ts = ts;
    item->flags  = flags;
    return ;
}

static
void vservice_dump(struct vservice* item)
{
    vassert(item);
    vdump(printf("-> SERVICE"));
    vdump(printf("category:%s", vpluger_get_desc(item->what)));
    vdump(printf("timestamp[rcv]: %s",  ctime(&item->rcv_ts)));
    vsockaddr_dump(&item->addr);
    vdump(printf("<- SERVICE"));
    return;
}

static
int _aux_mart_add_service_cb(void* item, void* cookie)
{
    struct vservice* svc = (struct vservice*)item;
    varg_decl(cookie, 0, vsrvcInfo*, svci);
    varg_decl(cookie, 1, struct vservice**, to);
    varg_decl(cookie, 2, time_t*, now);
    varg_decl(cookie, 3, int32_t*, min_tdff);
    varg_decl(cookie, 4, int*, found);

    if ((svc->what == svci->usage)
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
int _aux_mart_get_service_cb(void* item, void* cookie)
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

static
int _vroute_mart_add_service_node(struct vroute_mart* mart, vsrvcInfo* svci)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int32_t min_tdff = (int32_t)0x7fffffff;
    int found = 0;
    int updt = 0;
    int ret  = -1;

    vassert(mart);
    vassert(svci);
    retE((svci->usage < 0));
    retE((svci->usage >= PLUGIN_BUTT));

    svcs = &mart->bucket[svci->usage].srvcs;
    {
        void* argv[] = {
            svci,
            &to,
            &now,
            &min_tdff,
            &found
        };
        varray_iterate(svcs, _aux_mart_add_service_cb, argv);
        if (found) {
            to->rcv_ts = now;
            updt = 1;
        } else if (to) {
            vservice_init(to, svci->usage, &svci->addr, now, 0);
            updt = 1;
        } else {
            to = vservice_alloc();
            retE((!to));
            vservice_init(to, svci->usage, &svci->addr, now, 0);
            varray_add_tail(svcs, to);
            updt = 1;
        };
    }
    if (updt) {
        mart->bucket[svci->usage].ts = now;
        ret = 0;
    }
    return ret;
}

static
int _vroute_mart_get_serivce_node(struct vroute_mart* mart, int what, vsrvcInfo* svci)
{
    struct varray* svcs = NULL;
    struct vservice* to = NULL;
    time_t now = time(NULL);
    int32_t min_tdff = (int32_t)0x7fffffff;
    int ret = -1;

    vassert(mart);
    vassert(svci);

    svcs = &mart->bucket[what].srvcs;
    {
        void* argv[] = {
            &to,
            &now,
            &min_tdff
        };
        varray_iterate(svcs, _aux_mart_get_service_cb, argv);
        if (to) {
            vsrvcInfo_init(svci, what, &to->addr);
            ret = 0;
        }
    }
    return ret;
}

/*
 * clear all plugs
 * @pluger: handler to plugin structure.
 */
static
void _vroute_mart_clear(struct vroute_mart* mart)
{
    struct varray* svcs = NULL;
    int i = 0;
    vassert(mart);

    for (; i < PLUGIN_BUTT; i++) {
        svcs = &mart->bucket[i].srvcs;
        while(varray_size(svcs) > 0) {
            vservice_free((struct vservice*)varray_del(svcs, 0));
        }
    }
    return;
}

/*
 * dump pluger
 * @pluger: handler to plugin structure.
 */
static
void _vroute_mart_dump(struct vroute_mart* mart)
{
    struct varray* svcs = NULL;
    int i = 0;
    int j = 0;

    vassert(mart);

    vdump(printf("-> ROUTING MART"));
    for (i = 0; i < PLUGIN_BUTT; i++) {
        svcs = &mart->bucket[i].srvcs;
        for (j = 0; j < varray_size(svcs); j++) {
            vservice_dump((struct vservice*)varray_get(svcs, j));
        }
    }
    vdump(printf("<- ROUTING MART"));
    return;
}

static
struct vroute_mart_ops route_mart_ops = {
    .add_srvc_node = _vroute_mart_add_service_node,
    .get_srvc_node = _vroute_mart_get_serivce_node,
    .clear         = _vroute_mart_clear,
    .dump          = _vroute_mart_dump
};

int vroute_mart_init(struct vroute_mart* mart, struct vconfig* cfg)
{
    int i = 0;

    vassert(mart);
    vassert(cfg);

    for (; i < PLUGIN_BUTT; i++) {
        varray_init(&mart->bucket[i].srvcs, 8);
        mart->bucket[i].ts = 0;
    }
    cfg->ops->get_int_ext(cfg, "route.bucket_size", &mart->bucket_sz, DEF_ROUTE_BUCKET_CAPC);
    mart->ops = &route_mart_ops;

    return 0;
}

void vroute_mart_deinit(struct vroute_mart* mart)
{
    int i = 0;

    mart->ops->clear(mart);
    for (; i < PLUGIN_BUTT; i++) {
        varray_deinit(&mart->bucket[i].srvcs);
    }
    return ;
}

