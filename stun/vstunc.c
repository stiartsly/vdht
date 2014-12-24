#include "vglobal.h"
#include "vstun_proto.h"
#include "vstunc.h"

extern struct vstun_proto_ops stun_proto_ops;
struct vstunc_req {
    struct vlist list;
    uint8_t trans_id[12];
    handle_mapped_addr_t cb;
    void* cookie;
};

static MEM_AUX_INIT(stunc_req_cache, sizeof(struct vstunc_req), 4);
static
struct vstunc_req* vstunc_req_alloc(void)
{
    struct vstunc_req* req = NULL;

    req = (struct vstunc_req*)vmem_aux_alloc(&stunc_req_cache);
    vlog((!req), elog_vmem_aux_alloc);
    retE_p((!req));

    memset(req, 0, sizeof(*req));
    return req;
}

static
void vstunc_req_free(struct vstunc_req* req)
{
    vassert(req);
    vmem_aux_free(&stunc_req_cache, req);

    return ;
}

static
void vstunc_req_init(struct vstunc_req* req, uint8_t* trans_id, handle_mapped_addr_t cb, void* cookie)
{
    vlist_init(&req->list);
    memcpy(req->trans_id, trans_id, 12);
    req->cb = cb;
    req->cookie = cookie;

    return ;
}

struct vstunc_srv {
    struct vlist list;
    struct sockaddr_in addr;
    char hostname[32];
};

static MEM_AUX_INIT(stunc_srv_cache, sizeof(struct vstunc_srv), 4);
static
struct vstunc_srv* vstunc_srv_alloc(void)
{
    struct vstunc_srv* srv = NULL;

    srv = (struct vstunc_srv*)vmem_aux_alloc(&stunc_srv_cache);
    vlog((!srv), elog_vmem_aux_alloc);
    retE_p((!srv));

    memset(srv, 0, sizeof(*srv));
    return srv;
}

static
void vstunc_srv_free(struct vstunc_srv* srv)
{
    vassert(srv);
    vmem_aux_free(&stunc_srv_cache, srv);

    return ;
}

static
void vstunc_srv_init(struct vstunc_srv* srv, char* hostname, struct sockaddr_in* addr)
{
    vassert(srv);
    vassert(hostname);

    vlist_init(&srv->list);
    strncpy(srv->hostname, hostname, 32);
    vsockaddr_copy(&srv->addr, addr);

    return;
}

static
int _vstunc_add_server(struct vstunc* stunc, char* srv_name, struct sockaddr_in* srv_addr)
{
    struct vstunc_srv* srv = NULL;

    vassert(stunc);
    vassert(srv_name);
    vassert(srv_addr);

    srv = vstunc_srv_alloc();
    vlog((!srv), elog_vstunc_srv_alloc);
    retE((!srv));
    vstunc_srv_init(srv, srv_name, srv_addr);

    vlock_enter(&stunc->lock);
    vlist_add_tail(&stunc->servers, &srv->list);
    vlock_leave(&stunc->lock);

    return 0;
}

static
void _aux_trans_id_make(uint8_t* trans_id)
{
    int i = 0;
    vassert(trans_id);

    srand(time(NULL));
    for (; i < 12; i++) {
        trans_id[i] = (uint8_t)(rand() % 16);
    }
    return ;
}

static
int _vstunc_req_mapped_addr(struct vstunc* stunc, struct sockaddr_in* to, handle_mapped_addr_t cb, void* cookie)
{
    struct vstunc_req* req = NULL;
    struct vstun_msg msg;
    void* buf = NULL;
    uint8_t trans_id[12];
    int ret = 0;

    vassert(stunc);
    vassert(cb);

    buf = malloc(BUF_SZ);
    vlog((!buf), elog_malloc);
    retE((!buf));

    memset(&msg, 0, sizeof(msg));
    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = 0;
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->proto_ops->encode(&msg, buf, BUF_SZ);
    ret1E((ret < 0), free(buf));

    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(to),
            .msgId = VMSG_STUN,
            .data  = buf,
            .len   = ret
        };
        ret = stunc->msger->ops->push(stunc->msger, &msg);
        ret1E((ret < 0), free(buf));
    }

    req = vstunc_req_alloc();
    vlog((!req), elog_vstunc_req_alloc);
    retE((!req));

    vstunc_req_init(req, trans_id, cb, cookie);
    vlock_enter(&stunc->lock);
    vlist_add_tail(&stunc->items, &req->list);
    vlock_leave(&stunc->lock);

    return 0;
}

static
int _vstunc_rsp_mapped_addr(struct vstunc* stunc, struct sockaddr_in* from, char* buf, int len)
{
    struct vstun_msg msg;
    struct vstun_msg_header* hdr = &msg.header;
    struct vstunc_req* req = NULL;
    struct vlist* node = NULL;
    int ret = 0;

    vassert(stunc);
    vassert(buf);
    vassert(len > 0);
    vassert(from);

    memset(&msg, 0, sizeof(msg));
    ret = stunc->proto_ops->decode(buf, len, &msg);
    retE((ret < 0));
    retS((STUN_MAGIC != hdr->magic)); //skip non-stun message.

    vlock_enter(&stunc->lock);
    __vlist_for_each(node, &stunc->items) {
        req = vlist_entry(node, struct vstunc_req, list);
        if (!memcmp(req->trans_id, hdr->trans_id, 12)) {
            vlist_del(&req->list);
            break;
        }
        req = NULL;
    }
    vlock_leave(&stunc->lock);
    retS((!req)); // skip wrong trans_id message.

    switch(hdr->type) {
    case msg_bind_rsp: {
        struct vattr_addrv4* addr = &msg.mapped_addr;
        struct sockaddr_in sin_addr;

        if (msg.has_mapped_addr) {
            vsockaddr_convert2(addr->addr, addr->port, &sin_addr);
            req->cb(&sin_addr, req->cookie);
        }
        break;
    }
    case msg_bind_rsp_err:
        break;
    default:
        retE((1));
        break;
    }
    if (req) {
        vstunc_req_free(req);
    }
    return 0;
}

static
int _vstunc_get_nat_type(struct vstunc* stunc)
{
    vassert(stunc);

    //todo;
    return 0;
}

static
int _vstunc_clear(struct vstunc* stunc)
{
    struct vstunc_srv* srv = NULL;
    struct vstunc_req*   item = NULL;
    struct vlist* node = NULL;
    vassert(stunc);

    vlock_enter(&stunc->lock);
    while(!vlist_is_empty(&stunc->items)) {
        node = vlist_pop_head(&stunc->items);
        item = vlist_entry(node, struct vstunc_req, list);
        vstunc_req_free(item);
    }
    while(!vlist_is_empty(&stunc->servers)) {
        node = vlist_pop_head(&stunc->servers);
        srv  = vlist_entry(node, struct vstunc_srv, list);
        vstunc_srv_free(srv);
    }
    vlock_leave(&stunc->lock);
    return 0;
}

struct vstunc_ops stunc_ops = {
    .add_server      = _vstunc_add_server,
    .req_mapped_addr = _vstunc_req_mapped_addr,
    .rsp_mapped_addr = _vstunc_rsp_mapped_addr,
    .get_nat_type    = _vstunc_get_nat_type,
    .clear           = _vstunc_clear
};

static
int _aux_stunc_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    int ret = 0;

    vassert(stunc);
    vassert(mu);

    ret = stunc->ops->rsp_mapped_addr(stunc, to_sockaddr_sin(mu->addr), mu->data, mu->len);
    retE((ret < 0));
    return 0;
}

int vstunc_init(struct vstunc* stunc, struct vmsger* msger)
{
    vassert(stunc);

    vlist_init(&stunc->items);
    vlock_init(&stunc->lock);

    stunc->ops       = &stunc_ops;
    stunc->proto_ops = &stun_proto_ops;
    msger->ops->add_cb(msger, stunc, _aux_stunc_msg_cb, VMSG_STUN);
    return 0;
}

void vstunc_deinit(struct vstunc* stunc)
{
    vassert(stunc);

    stunc->ops->clear(stunc);
    vlock_deinit(&stunc->lock);

    return ;
}

