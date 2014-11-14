#include "vglobal.h"
#include "vplugin.h"

enum {
    VPLUGIN_GET_PLUGIN,
    VPLUGIN_GET_PLUGIN_RSP,
    VPLUGIN_BUTT
};

static char* plugin_desc[] = {
    "relay",
    "stun",
    "vpn",
    "ddns",
    "multi_route",
    "data_hash",
    "app",
    NULL
};

/*
 * for plug request block
 *
 */
static MEM_AUX_INIT(plgn_req_cache, sizeof(struct vplgn_req), 8);
static
struct vplgn_req* vplgn_req_alloc(void)
{
    struct vplgn_req* req = NULL;

    req = (struct vplgn_req*)vmem_aux_alloc(&plgn_req_cache);
    vlog((!req), elog_vmem_aux_alloc);
    retE_p((!req));
    return req;
}

static
void vplgn_req_free(struct vplgn_req* req)
{
    vassert(req);
    vmem_aux_free(&plgn_req_cache, req);
    return ;
}

static
void vplgn_req_init(struct vplgn_req* req, int plgnId, vtoken* token, get_addr_cb_t cb, void* cookie)
{
    vassert(req);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);
    vassert(cb);

    vlist_init(&req->list);
    vtoken_copy(&req->token, token);
    req->plgnId = plgnId;
    req->cb     = cb;
    req->cookie = cookie;
    return;
}

/*
 * helpers to manage the msg buf for plugin dht request and response.
 */
static
void* _aux_mbuf_alloc(struct vpluger* pluger)
{
    void* mbuf = NULL;
    vassert(pluger);

    mbuf = malloc(8*BUF_SZ);
    vlog((!mbuf), elog_malloc);
    retE_p((!mbuf));
    memset(mbuf, 0, 8*BUF_SZ);

    //reserve padding for magic and msgId.
    return mbuf + 8;
}

static
int _aux_mbuf_len(void)
{
    return (int)(BUF_SZ - 8);
}

static
void _aux_mbuf_free(struct vpluger* pluger, void* mbuf)
{
    vassert(pluger);
    vassert(mbuf);

    free(mbuf - 8);
    return ;
}

static
int _aux_req_get_plugin(struct vpluger* pluger, vtoken* token, int plgnId, struct vsockaddr* addr)
{
    void* buf = NULL;
    int ret = 0;

    vassert(pluger);
    vassert(addr);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    buf = _aux_mbuf_alloc(pluger);
    retE((!buf));

    ret = dht_enc_ops.get_plugin(token, plgnId, buf, _aux_mbuf_len());
    ret1E((ret < 0), _aux_mbuf_free(pluger, buf));

    {
        struct vmsg_usr msg = {
            .addr  = addr,
            .msgId = VMSG_PLUG,
            .data  = buf,
            .len   = ret
        };

        ret = pluger->msger->ops->push(pluger->msger, &msg);
        ret1E((ret < 0), _aux_mbuf_free(pluger, buf));
    }
    return 0;
}

/*
 * the routine to send @get_plugin request to acquire address info of given
 * plugin server.
 *
 * @pluger:
 * @plgnId: plug type.
 * @cb    : callback to be invoked when @get_plugin_rsp arrived, which brings
 *          address info about given plugin @plugId
 * @cookie: cookie to @cb.
 *
 */
static
int _vpluger_c_req(struct vpluger* pluger, int plgnId, get_addr_cb_t cb, void* cookie)
{
    struct vplgn_req* req = NULL;
    vnodeAddr dest;
    vtoken token;
    int ret = 0;

    vassert(pluger);
    vassert(cb);

    retE((plgnId < 0));
    retE((plgnId >= PLUGIN_BUTT));

    ret = pluger->route->plugin_ops->get(pluger->route, plgnId, &dest);
    retE((ret < 0));

    req = vplgn_req_alloc();
    vlog((!req), elog_vplgn_req_alloc);
    vtoken_make(&token);
    vplgn_req_init(req, plgnId, &token, cb, cookie);

    ret = _aux_req_get_plugin(pluger, &token, plgnId, to_vsockaddr_from_sin(&dest.addr));
    ret1E((ret < 0), vplgn_req_free(req));

    vlock_enter(&pluger->prq_lock);
    vlist_add_tail(&pluger->prqs, &req->list);
    vlock_leave(&pluger->prq_lock);

    return 0;
}

/*
 * the routine to deal with dht message @get_plugin_rsp from
 *
 * @pluger:
 * @plgnId: plugin ID
 * @addr  : addr to given plugin server @plgnId
 *
 */
static
int _vpluger_c_invoke_req(struct vpluger* pluger, int plgnId, vtoken* token, struct sockaddr_in* addr)
{
    struct vplgn_req* req = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(token);
    vassert(addr);

    retE((plgnId < 0));
    retE((plgnId >= PLUGIN_BUTT));

    vlock_enter(&pluger->prq_lock);
    __vlist_for_each(node, &pluger->prqs) {
        req = vlist_entry(node, struct vplgn_req, list);
        if (req->plgnId == plgnId) {
            vlist_del(&req->list);
            found = 1;
            break;

        }
    }
    vlock_leave(&pluger->prq_lock);
    if (found) {
        req->cb(addr, req->cookie);
        vplgn_req_free(req);
    }
    return (found ? 0: -1);
}

/*
 * clear all plugs
 * @pluger:
 */
static
int _vpluger_c_clear(struct vpluger* pluger)
{
    struct vplgn_req* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vlock_enter(&pluger->prq_lock);
    while(!vlist_is_empty(&pluger->prqs)) {
        node = vlist_pop_head(&pluger->prqs);
        item = vlist_entry(node, struct vplgn_req, list);
        vplgn_req_free(item);
    }
    vlock_leave(&pluger->prq_lock);
    return 0;
}

/*
 * dump
 * @pluger:
 */
static
void _vpluger_c_dump(struct vpluger* pluger)
{
    struct vplgn_req* req = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vdump(printf("-> PLUGIN_REQS"));
    vlock_enter(&pluger->prq_lock);
    __vlist_for_each(node, &pluger->prqs) {
        req = vlist_entry(node, struct vplgn_req, list);
        vdump(printf("-> PLUGIN_REQ"));
        vtoken_dump(&req->token);
        vdump(printf("plgnId: %s", plugin_desc[req->plgnId]));
        vdump(printf("<- PLUGIN_REQ"));
    }
    vlock_leave(&pluger->prq_lock);
    vdump(printf("<- PLUGIN_REQS"));

    return;
}

/*
 * plug ops set as client side.
 */
static
struct vpluger_c_ops pluger_c_ops = {
    .req    = _vpluger_c_req,
    .invoke = _vpluger_c_invoke_req,
    .clear  = _vpluger_c_clear,
    .dump   = _vpluger_c_dump
};

/*
 * for plug_item
 */
static MEM_AUX_INIT(plgn_item_cache, sizeof(struct vplgn_item), 8);
static
struct vplgn_item* vplgn_item_alloc(void)
{
    struct vplgn_item* item = NULL;

    item = (struct vplgn_item*)vmem_aux_alloc(&plgn_item_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    return item;
}

static
void vplgn_item_free(struct vplgn_item* item)
{
    vassert(item);
    vmem_aux_free(&plgn_item_cache, item);
    return ;
}

static
void vplgn_item_init(struct vplgn_item* item, int plgnId, struct sockaddr_in* addr)
{
    vassert(item);
    vassert(addr);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    vlist_init(&item->list);
    vsockaddr_copy(&item->addr, addr);
    item->plgnId = plgnId;
    return;
}

/*
 * plug an server.
 * @pluger:
 * @plgnId: plugin ID
 * @addr  : addr to plugin server.
 */
static
int _vpluger_s_plug(struct vpluger* pluger, int plgnId, struct sockaddr_in* addr)
{
    struct vplgn_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plgnId < 0));
    retE((plgnId >= PLUGIN_BUTT));

    vlock_enter(&pluger->plgn_lock);
    __vlist_for_each(node, &pluger->plgns) {
        item = vlist_entry(node, struct vplgn_item, list);
        if ((item->plgnId == plgnId)
           && vsockaddr_equal(&item->addr, addr)) {
           found = 1;
           break;
        }
    }
    if (!found) {
        item = vplgn_item_alloc();
        vlog((!item), elog_vplgn_item_alloc);
        ret1E((!item), vlock_leave(&pluger->plgn_lock));

        vplgn_item_init(item, plgnId, addr);
        vlist_add_tail(&pluger->plgns, &item->list);
    }
    vlock_leave(&pluger->plgn_lock);
    return 0;
}

/*
 * unplug an server.
 * @pluger:
 * @plgnId: plugin ID
 * @addr:   addr to plugin server.
 */
static
int _vpluger_s_unplug(struct vpluger* pluger, int plgnId, struct sockaddr_in* addr)
{
    struct vplgn_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plgnId < 0));
    retE((plgnId >= PLUGIN_BUTT));

    vlock_enter(&pluger->plgn_lock);
    __vlist_for_each(node, &pluger->plgns) {
        item = vlist_entry(node, struct vplgn_item, list);
        if ((item->plgnId == plgnId)
           && vsockaddr_equal(&item->addr, addr)) {
           found = 1;
           break;
        }
    }
    if (found) {
        vlist_del(&item->list);
        free(item);
    }
    vlock_leave(&pluger->plgn_lock);
    return 0;
}

/*
 * the routine to get address info by given plugin ID
 * @pluger:
 * @plgnId: plugin ID
 * @addr  : [out]
 */
static
int _vpluger_s_get(struct vpluger* pluger, int plgnId, struct sockaddr_in* addr)
{
    struct vplgn_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(pluger);
    vassert(addr);

    retE((plgnId < 0));
    retE((plgnId >= PLUGIN_BUTT));

    vlock_enter(&pluger->plgn_lock);
    __vlist_for_each(node, &pluger->plgns) {
        item = vlist_entry(node, struct vplgn_item, list);
        if (item->plgnId == plgnId) {
            found = 1;
            break;
        }
    }
    if (found) {
        vsockaddr_copy(addr, &item->addr);
    }
    vlock_leave(&pluger->plgn_lock);
    return (found ? 0 : -1);
}

/*
 * the routine that plugin deals with reqeust @get_plugin when request arrived.
 * @pluger: handler to plugin structure.
 * @plgnId: plugin Id
 * @token : token to reqeust @get_plugin
 * @to    : addr  where reqeust was from.
 */
static
int _vpluger_s_rsp(struct vpluger* pluger, int plgnId, vtoken* token, struct vsockaddr* to)
{
    struct sockaddr_in addr;
    void* buf = NULL;
    int ret = 0;

    vassert(pluger);
    vassert(token);
    vassert(to);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    ret = pluger->s_ops->get(pluger, plgnId, &addr);
    retE((ret < 0));

    buf = _aux_mbuf_alloc(pluger);
    retE((!buf));
    ret = dht_enc_ops.get_plugin_rsp(token, plgnId, &addr, buf, _aux_mbuf_len());
    ret1E((ret < 0), _aux_mbuf_free(pluger, buf));

    {
        struct vmsg_usr msg = {
            .addr = to,
            .msgId = VMSG_PLUG,
            .data  = buf,
            .len   = ret
        };
        ret = pluger->msger->ops->push(pluger->msger, &msg);
        ret1E((ret < 0), _aux_mbuf_free(pluger, buf));
    }
    return 0;
}

/*
 * clear all plugs
 * @pluger: handler to plugin structure.
 */
static
int _vpluger_s_clear(struct vpluger* pluger)
{
    struct vplgn_item* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vlock_enter(&pluger->plgn_lock);
    while(!vlist_is_empty(&pluger->plgns)) {
        node = vlist_pop_head(&pluger->plgns);
        item = vlist_entry(node, struct vplgn_item, list);
        vplgn_item_free(item);
    }
    vlock_leave(&pluger->plgn_lock);
    return 0;
}

/*
 * dump pluger
 * @pluger: handler to plugin structure.
 */
static
void _vpluger_s_dump(struct vpluger* pluger)
{
    struct vplgn_item* item = NULL;
    struct vlist* node = NULL;
    vassert(pluger);

    vdump(printf("-> PLUGINS"));
    vlock_enter(&pluger->plgn_lock);
    __vlist_for_each(node, &pluger->plgns) {
        item = vlist_entry(node, struct vplgn_item, list);
        vdump(printf("-> PLUGIN"));
        vdump(printf("plugId: %s", plugin_desc[item->plgnId]));
        vsockaddr_dump(&item->addr);
        vdump(printf("<- PLUGIN"));
    }
    vlock_leave(&pluger->plgn_lock);
    vdump(printf("<- PLUGINS"));

    return;
}

/*
 * plug ops set as srever side.
 */
static
struct vpluger_s_ops pluger_s_ops = {
    .plug   = _vpluger_s_plug,
    .unplug = _vpluger_s_unplug,
    .get    = _vpluger_s_get,
    .rsp    = _vpluger_s_rsp,
    .clear  = _vpluger_s_clear,
    .dump   = _vpluger_s_dump
};

/*
 * the callback to deal the dht message about plugin.
 * @cookie:
 * @mu:
 */
static
int _vpluger_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vdht_dec_ops* dec_ops = &dht_dec_ops;
    struct vpluger* pluger = (struct vpluger*)cookie;
    struct sockaddr_in addr;
    vtoken token;
    void* ctxt = NULL;
    int plgnId = 0;
    int ret = 0;

    vassert(pluger);
    vassert(mu);

    ret = dec_ops->dec(mu->data, mu->len, &ctxt);
    retE((ret < 0));

    switch(ret) {
    case VPLUGIN_GET_PLUGIN: {
        ret = dec_ops->get_plugin(ctxt, &token, &plgnId);
        dec_ops->dec_done(ctxt);
        retE((ret < 0));

        ret = pluger->s_ops->rsp(pluger, plgnId, &token, mu->addr);
        retE((ret < 0));
        break;
    }
    case VPLUGIN_GET_PLUGIN_RSP: {
        ret = dec_ops->get_plugin_rsp(ctxt, &token, &plgnId, &addr);
        dec_ops->dec_done(ctxt);
        retE((ret < 0));

        ret = pluger->c_ops->invoke(pluger, plgnId, &token, &addr);
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

    vlist_init(&pluger->plgns);
    vlock_init(&pluger->plgn_lock);
    vlist_init(&pluger->prqs);
    vlock_init(&pluger->prq_lock);

    pluger->route = &host->route;
    pluger->msger = &host->msger;
    pluger->c_ops = &pluger_c_ops;
    pluger->s_ops = &pluger_s_ops;

    pluger->msger->ops->add_cb(pluger->msger, pluger, _vpluger_msg_cb, VMSG_PLUG);
    return 0;
}

/*
 * pluger deinitialization.
 */
void vpluger_deinit(struct vpluger* pluger)
{
    vassert(pluger);

    pluger->c_ops->clear(pluger);
    vlock_deinit(&pluger->prq_lock);
    pluger->s_ops->clear(pluger);
    vlock_deinit(&pluger->plgn_lock);

    return ;
}

