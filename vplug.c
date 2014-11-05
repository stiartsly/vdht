#include "vglobal.h"
#include "vplug.h"

static
struct vplug_desc plug_desc[] = {
    {PLUG_RELAY, "relay"      },
    {PLUG_STUN,  "stun"       },
    {PLUG_VPN,   "vpn"        },
    {PLUG_DDNS,  "ddns"       },
    {PLUG_MROUTE,"multi route"},
    {PLUG_DHASH, "data hash"  },
    {PLUG_APP,   "app"        },
    {PLUG_BUTT,  0            }
};

/*
 * for plug request block
 *
 */
static MEM_AUX_INIT(plug_req_cache, sizeof(struct vplug_req), 8);
static
struct vplug_req* vplug_req_alloc(void)
{
    struct vplug_req* prq = NULL;

    prq = (struct vplug_req*)vmem_aux_alloc(&plug_req_cache);
    vlog((!prq), elog_vmem_aux_alloc);
    retE_p((!prq));
    return prq;
}

static
void vplug_req_free(struct vplug_req* prq)
{
    vassert(prq);
    vmem_aux_free(&plug_req_cache, prq);
    return ;
}

static
void vplug_req_init(struct vplug_req* prq, int plugId, get_addr_cb_t cb, void* cookie)
{
    vassert(prq);
    vassert(plugId >= 0);
    vassert(plugId < PLUG_BUTT);
    vassert(cb);

    vlist_init(&prq->list);
    prq->plugId = plugId;
    prq->cb     = cb;
    prq->cookie = cookie;
    vtoken_make(&prq->token);
    return;
}

/* message format for request and reply for special plug infos.
 * ---------------------------------------------
 * magic  | msgId  | propId | prop content
 * ---------------------------------------------
 *
 * 1. request special plug info
 * ---------------------------------------------
 * magic  | msgId  | req    | plugId
 * ---------------------------------------------
 * 2. reply for special plug info request.
 * ---------------------------------------------
 * magic  | msgId  | rsp    | plug info
 * ---------------------------------------------
 */
static
int _aux_req_cb(char* buf, void** cookie)
{
    varg_decl(cookie, 0, int, plugId);
    char* data = buf;
    int sz = 0;

    vassert(buf);
    vassert(cookie);

    set_int32(data, 0); //means request.
    sz += sizeof(long);
    data = offset_addr(data, sizeof(long));

    set_int32(data, plugId);
    sz += sizeof(long);

    return sz;
}

static
int _aux_rsp_cb(char* buf, void** cookie)
{
    varg_decl(cookie, 0, int, plugId);
    varg_decl(cookie, 1, struct sockaddr_in*, addr);
    char* data = buf;
    int sz = 0;

    vassert(buf);
    vassert(cookie);

    set_int32(data, 1); // means response;
    sz += sizeof(long);
    data = offset_addr(data, sizeof(long));

    set_int32(data, plugId);
    sz += sizeof(long);
    data = offset_addr(data, sizeof(long));

    set_int32(data, (long)addr->sin_port);
    sz += sizeof(long);
    data = offset_addr(data, sizeof(long));

    set_uint32(data, (uint32_t)addr->sin_addr.s_addr);
    sz += sizeof(uint32_t);

    return sz;
}

static
int _aux_encode_msg(char* buf, int buf_sz, int (*cb)(char*, void**), void** cookie)
{
    char* data = buf;
    int sz = 0;

    vassert(buf);
    vassert(buf_sz > 0);
    vassert(cb);

    set_uint32(data, VPLUG_MAGIC);
    sz += sizeof(uint32_t);
    data = offset_addr(data, sizeof(uint32_t));

    set_int32(data, VMSG_PLUG);
    sz += sizeof(long);
    data = offset_addr(data, sizeof(long));

    sz += cb(data, cookie);
    return sz;
}

/*
 * get server addr info from remote server.
 * @pluger:
 * @plugId:
 * @addr:
 */
static
int _vpluger_c_req(struct vpluger* pluger, int plugId, get_addr_cb_t cb, void* cookie)
{
    struct vplug_req* prq = NULL;
    vnodeAddr dest;
    char buf[64];
    int ret = 0;

    vassert(pluger);
    vassert(cb);

    retE((plugId < 0));
    retE((plugId >= PLUG_BUTT));

    ret = pluger->route->plug_ops->get(pluger->route, plugId, &dest);
    retE((ret < 0));

    prq = vplug_req_alloc();
    vlog((!prq), elog_vplug_req_alloc);
    retE((!prq));
    vplug_req_init(prq, plugId, cb, cookie);

    {
        void* args[] = { (void*)plugId };
        struct vmsg_usr msg = {
            .addr  = (struct vsockaddr*)&dest.addr,
            .msgId = VMSG_PLUG,
            .data  = buf,
            .len   = _aux_encode_msg(buf, 64, _aux_req_cb, args)
        };

        ret = pluger->msger->ops->push(pluger->msger, &msg);
        ret1E((ret < 0), vplug_req_free(prq));
    }

    vlock_enter(&pluger->prq_lock);
    vlist_add_tail(&pluger->prqs, &prq->list);
    vlock_leave(&pluger->prq_lock);

    return 0;
}

/*
 * clear all plugs
 */
static
int _vpluger_c_rsp(struct vpluger* pluger, int plugId, struct sockaddr_in* addr)
{
    struct vplug_req* prq = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plugId < 0));
    retE((plugId >= PLUG_BUTT));

    vlock_enter(&pluger->prq_lock);
    __vlist_for_each(node, &pluger->prqs) {
        prq = vlist_entry(node, struct vplug_req, list);
        if (prq->plugId == plugId) {
            vlist_del(&prq->list);
            found = 1;
            break;
        }
    }
    vlock_leave(&pluger->prq_lock);
    if (found) {
        prq->cb(addr, prq->cookie);
        vplug_req_free(prq);
    }
    return (found ? 0: -1);
}

/*
 * clear all plugs
 */
static
int _vpluger_c_clear(struct vpluger* pluger)
{
    struct vplug_req* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vlock_enter(&pluger->prq_lock);
    while(!vlist_is_empty(&pluger->prqs)) {
        node = vlist_pop_head(&pluger->prqs);
        item = vlist_entry(node, struct vplug_req, list);
        vplug_req_free(item);
    }
    vlock_leave(&pluger->prq_lock);
    return 0;
}

/*
 * dump
 */
static
int _vpluger_c_dump(struct vpluger* pluger)
{
    vassert(pluger);

    //todo;
    return 0;
}

/*
 * plug ops set as client side.
 */
static
struct vpluger_c_ops pluger_c_ops = {
    .req   = _vpluger_c_req,
    .rsp   = _vpluger_c_rsp,
    .clear = _vpluger_c_clear,
    .dump  = _vpluger_c_dump
};

/*
 * for plug_item
 */
static MEM_AUX_INIT(plug_item_cache, sizeof(struct vplug_item), 8);
static
struct vplug_item* vplug_item_alloc(void)
{
    struct vplug_item* item = NULL;

    item = (struct vplug_item*)vmem_aux_alloc(&plug_item_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    return item;
}

static
void vplug_item_free(struct vplug_item* item)
{
    vassert(item);
    vmem_aux_free(&plug_item_cache, item);
    return ;
}

static
void vplug_item_init(struct vplug_item* item, int plugId, struct sockaddr_in* addr)
{
    struct vplug_desc* desc = plug_desc;
    int i = 0;

    vassert(item);
    vassert(plugId >= 0);
    vassert(plugId < PLUG_BUTT);
    vassert(addr);

    vlist_init(&item->list);
    vsockaddr_copy(&item->addr, addr);
    item->plugId = plugId;

    for (; desc[i].desc; i++) {
        if (desc[i].plugId == plugId) {
            item->desc = desc[i].desc;
            break;
        }
    }
    return;
}


/*
 * plug an server
 * @pluger:
 * @plugId:
 * @addr  :
 */
static
int _vpluger_s_plug(struct vpluger* pluger, int plugId, struct sockaddr_in* addr)
{
    struct vplug_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plugId < 0));
    retE((plugId >= PLUG_BUTT));

    vlock_enter(&pluger->plug_lock);
    __vlist_for_each(node, &pluger->plugs) {
        item = vlist_entry(node, struct vplug_item, list);
        if ((item->plugId == plugId)
           && vsockaddr_equal(&item->addr, addr)) {
           found = 1;
           break;
        }
    }
    if (!found) {
        item = vplug_item_alloc();
        vlog((!item), elog_vplug_item_alloc);
        ret1E((!item), vlock_leave(&pluger->plug_lock));

        vplug_item_init(item, plugId, addr);
        vlist_add_tail(&pluger->plugs, &item->list);
    }
    vlock_leave(&pluger->plug_lock);
    return 0;
}

/*
 * unplug an server
 * @pluger:
 * @plugId:
 * @addr:
 */
static
int _vpluger_s_unplug(struct vpluger* pluger, int plugId, struct sockaddr_in* addr)
{
    struct vplug_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plugId < 0));
    retE((plugId >= PLUG_BUTT));

    vlock_enter(&pluger->plug_lock);
    __vlist_for_each(node, &pluger->plugs) {
        item = vlist_entry(node, struct vplug_item, list);
        if ((item->plugId == plugId)
           && vsockaddr_equal(&item->addr, addr)) {
           found = 1;
           break;
        }
    }
    if (found) {
        vlist_del(&item->list);
        free(item);
    }
    vlock_leave(&pluger->plug_lock);
    return 0;
}

/*
 *
 */
static
int _vpluger_s_get(struct vpluger* pluger, int plugId, struct sockaddr_in* addr)
{
    struct vplug_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plugId < 0));
    retE((plugId >= PLUG_BUTT));

    vlock_enter(&pluger->plug_lock);
    __vlist_for_each(node, &pluger->plugs) {
        item = vlist_entry(node, struct vplug_item, list);
        if (item->plugId == plugId) {
            found = 1;
            break;
        }
    }
    if (found) {
        vsockaddr_copy(addr, &item->addr);
    }
    vlock_leave(&pluger->plug_lock);
    return (found ? 0 : -1);
}

/*
 * clear all plugs
 */
static
int _vpluger_s_clear(struct vpluger* pluger)
{
    struct vplug_item* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vlock_enter(&pluger->plug_lock);
    while(!vlist_is_empty(&pluger->plugs)) {
        node = vlist_pop_head(&pluger->plugs);
        item = vlist_entry(node, struct vplug_item, list);
        vplug_item_free(item);
    }
    vlock_leave(&pluger->plug_lock);
    return 0;
}

/*
 * dump pluger
 * @pluger:
 */
static
int _vpluger_s_dump(struct vpluger* pluger)
{
    vassert(pluger);

    //todo;
    return 0;
}

/*
 * plug ops set as srever side.
 */
static
struct vpluger_s_ops pluger_s_ops = {
    .plug   = _vpluger_s_plug,
    .unplug = _vpluger_s_unplug,
    .get    = _vpluger_s_get,
    .clear  = _vpluger_s_clear,
    .dump   = _vpluger_s_dump
};

static
int _vpluger_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vpluger* pluger = (struct vpluger*)cookie;
    struct sockaddr_in addr;
    int plugId = 0;
    int type   = 0;
    int ret = 0;

    vassert(pluger);
    vassert(mu);

    type   = get_int32(mu->data);
    plugId = get_int32(mu->data + 4);
    switch(type) {
    case 0: { //means request.
        char buf[64];
        ret = pluger->s_ops->get(pluger, plugId, &addr);
        retE((ret < 0));
        {
            void* args[] = { (void*)plugId, &addr};
            struct vmsg_usr msg = {
                .addr  = (struct vsockaddr*)&addr,
                .msgId = VMSG_PLUG,
                .data  = buf,
                .len   = _aux_encode_msg(buf, 64, _aux_rsp_cb, args)
            };
            ret = pluger->msger->ops->push(pluger->msger, &msg);
            retE((ret < 0));
        }
        break;
    }
    case 1: { //means response.
        addr.sin_port = get_int32(mu->data + 8);
        addr.sin_addr.s_addr = get_uint32(mu->data + 12);

        ret = pluger->c_ops->rsp(pluger, plugId, &addr);
        retE((ret < 0));
        break;
    }
    default:
        retE((1));
        break;
    }
    return 0;
}

/*
 * pluger initialization
 */
int vpluger_init(struct vpluger* pluger, struct vhost* host)
{
    vassert(pluger);
    vassert(host);

    vlist_init(&pluger->plugs);
    vlock_init(&pluger->plug_lock);
    vlist_init(&pluger->prqs);
    vlock_init(&pluger->prq_lock);

    pluger->route = &host->node.route;
    pluger->msger = &host->msger;
    pluger->c_ops = &pluger_c_ops;
    pluger->s_ops = &pluger_s_ops;

    pluger->msger->ops->add_cb(pluger->msger, pluger, _vpluger_msg_cb, VMSG_PLUG);
    return 0;
}

/*
 *
 */
void vpluger_deinit(struct vpluger* pluger)
{
    vassert(pluger);

    pluger->c_ops->clear(pluger);
    vlock_deinit(&pluger->prq_lock);
    pluger->s_ops->clear(pluger);
    vlock_deinit(&pluger->plug_lock);

    return ;
}
