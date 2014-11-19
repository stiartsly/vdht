#include "vglobal.h"
#include "vplugin.h"

struct vplugin_desc {
    int   plgnId;
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

char* vpluger_get_desc(int plugin_id)
{
    struct vplugin_desc* desc = plugin_desc;
    int i = 0;

    if ((plugin_id < PLUGIN_RELAY) || (plugin_id >= PLUGIN_BUTT)) {
        return "unknown plugin";
    }

    for (; desc->desc; i++) {
        if (desc->plgnId == plugin_id) {
            break;
        }
        desc++;
    }
    return desc->desc;
}

/*
 * for plug_item
 */
struct vservice_item {
    struct vlist list;
    vserviceId id;
    int what;
    struct sockaddr_in addr;
};

static MEM_AUX_INIT(service_cache, sizeof(struct vservice_item), 8);
static
struct vservice_item* vservice_alloc(void)
{
    struct vservice_item* item = NULL;

    item = (struct vservice_item*)vmem_aux_alloc(&service_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    return item;
}

static
void vservice_free(struct vservice_item* item)
{
    vassert(item);
    vmem_aux_free(&service_cache, item);
    return ;
}

static
void vservice_init(struct vservice_item* item, int what, struct sockaddr_in* addr)
{
    vassert(item);
    vassert(addr);

    vlist_init(&item->list);
    vserviceId_make(&item->id);
    item->what = what;
    vsockaddr_copy(&item->addr, addr);
    return ;
}

static
void vservice_dump(struct vservice_item* item)
{
    vassert(item);
    vdump(printf("-> SERVICE"));
    vserviceId_dump(&item->id);
    vdump(printf("category:%s", vpluger_get_desc(item->what)));
    vsockaddr_dump(&item->addr);
    vdump(printf("<- SERVICE"));
    return;
}

static
int _vpluger_get_service(struct vpluger* pluger, int what, struct sockaddr_in* addr)
{
    vassert(pluger);
    vassert(addr);

    //todo;
    return 0;
}
/*
 * plug an service.
 * @pluger:
 * @plgnId: plugin ID
 * @addr  : addr to plugin server.
 */
static
int _vpluger_reg_service(struct vpluger* pluger, int what, struct sockaddr_in* addr)
{
    struct vservice_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((what < 0));
    retE((what >= PLUGIN_BUTT));

    vlock_enter(&pluger->lock);
    __vlist_for_each(node, &pluger->services) {
        item = vlist_entry(node, struct vservice_item, list);
        if ((item->what == what)
           && vsockaddr_equal(&item->addr, addr)) {
           found = 1;
           break;
        }
    }
    if (!found) {
        item = vservice_alloc();
        vlog((!item), elog_vservice_alloc);
        ret1E((!item), vlock_leave(&pluger->lock));

        vservice_init(item, what, addr);
        vlist_add_tail(&pluger->services, &item->list);
    }
    vlock_leave(&pluger->lock);
    return 0;
}

/*
 * unplug an service.
 * @pluger:
 * @plgnId: plugin ID
 * @addr:   addr to plugin server.
 */
static
int _vpluger_unreg_service(struct vpluger* pluger, int what, struct sockaddr_in* addr)
{
    struct vservice_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((what < 0));
    retE((what >= PLUGIN_BUTT));

    vlock_enter(&pluger->lock);
    __vlist_for_each(node, &pluger->services) {
        item = vlist_entry(node, struct vservice_item, list);
        if ((item->what == what)
           && vsockaddr_equal(&item->addr, addr)) {
           found = 1;
           break;
        }
    }
    if (found) {
        vlist_del(&item->list);
        vservice_free(item);
    }
    vlock_leave(&pluger->lock);
    return 0;
}

/*
 * clear all plugs
 * @pluger: handler to plugin structure.
 */
static
int _vpluger_clear(struct vpluger* pluger)
{
    struct vservice_item* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vlock_enter(&pluger->lock);
    while(!vlist_is_empty(&pluger->services)) {
        node = vlist_pop_head(&pluger->services);
        item = vlist_entry(node, struct vservice_item, list);
        vservice_free(item);
    }
    vlock_leave(&pluger->lock);
    return 0;
}

/*
 * dump pluger
 * @pluger: handler to plugin structure.
 */
static
void _vpluger_dump(struct vpluger* pluger)
{
    struct vservice_item* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vdump(printf("-> SERVICES"));
    vlock_enter(&pluger->lock);
    __vlist_for_each(node, &pluger->services) {
        item = vlist_entry(node, struct vservice_item, list);
        vservice_dump(item);
    }
    vlock_leave(&pluger->lock);
    vdump(printf("<- SERVICES"));
    return;
}

/*
 * pluger ops
 */
static
struct vpluger_ops pluger_ops = {
    .reg_service   = _vpluger_reg_service,
    .unreg_service = _vpluger_unreg_service,
    .get_service   = _vpluger_get_service,
    .clear  = _vpluger_clear,
    .dump   = _vpluger_dump
};

/*
 * pluger initialization
 */
int vpluger_init(struct vpluger* pluger, struct vroute* route)
{
    vassert(pluger);
    vassert(route);

    vlist_init(&pluger->services);
    vlock_init(&pluger->lock);

    pluger->route = route;
    pluger->ops   = &pluger_ops;
    return 0;
}

/*
 * pluger deinitialization.
 */
void vpluger_deinit(struct vpluger* pluger)
{
    vassert(pluger);

    pluger->ops->clear(pluger);
    vlock_deinit(&pluger->lock);
    return ;
}

