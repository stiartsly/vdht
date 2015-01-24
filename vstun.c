#include "vglobal.h"
#include "vstun_proto.h"
#include "vstun.h"

extern struct vstun_proto_ops stun_proto_ops;
struct vstun_req_item {
    struct vlist list;
    uint8_t trans_id[12];
    get_ext_addr_t cb;
    void*  cookie;
    time_t snd_ts;
};

static MEM_AUX_INIT(stun_req_cache, sizeof(struct vstun_req_item), 4);
static
struct vstun_req_item* vstun_req_alloc(void)
{
    struct vstun_req_item* req = NULL;
    req = (struct vstun_req_item*)vmem_aux_alloc(&stun_req_cache);
    vlog((!req), elog_vmem_aux_alloc);
    retE_p((!req));

    memset(req, 0, sizeof(*req));
    return req;
}

static
void vstun_req_free(struct vstun_req_item* req)
{
    vassert(req);
    vmem_aux_free(&stun_req_cache, req);

    return ;
}

static
void vstun_req_init(struct vstun_req_item* req, uint8_t* trans_id, get_ext_addr_t cb, void* cookie)
{
    vassert(req);
    vassert(trans_id);
    vassert(cb);

    memcpy(&req->trans_id, trans_id, 12);
    req->cb = cb;
    req->cookie = cookie;

    return ;
}

static
int _aux_snd_msg(struct vstun* stun, struct vstun_msg* msg, struct sockaddr_in* to)
{
    void* buf = NULL;
    int ret = 0;

    buf = malloc(BUF_SZ);
    vlog((!buf), elog_malloc);
    retE((!buf));

    ret = stun->proto_ops->encode(msg, buf, BUF_SZ);
    ret1E((ret < 0), free(buf));

    {
        struct vmsg_usr msg = {
            .addr  = to_vsockaddr_from_sin(to),
            .msgId = VMSG_STUN,
            .data  = buf,
            .len   = ret
        };
        ret = stun->msger->ops->push(stun->msger, &msg);
        ret1E((ret < 0), free(buf));
    }
    return 0;
}

static
int _vstun_snd_req_msg(struct vstun* stun, struct vstun_msg* msg, struct sockaddr_in* to, get_ext_addr_t cb, void* cookie)
{
    struct vstun_req_item* req = NULL;
    int ret = 0;

    vassert(stun);
    vassert(msg);
    vassert(to);
    vassert(msg_bind_req == msg->header.type);

    ret = _aux_snd_msg(stun, msg, to);
    retE((ret < 0));

    req = vstun_req_alloc();
    retE((!req));

    vstun_req_init(req, msg->header.trans_id, cb, cookie);

    vlock_enter(&stun->lock);
    vlist_add_tail(&stun->reqs, &req->list);
    vlock_leave(&stun->lock);

    return 0;
}

static
int _vstun_rcv_req_msg(struct vstun* stun, struct vstun_msg* msg, struct sockaddr_in* from)
{
    struct vstun_msg rsp_msg;
    int ret = 0;

    vassert(stun);
    vassert(msg);
    vassert(from);
    vassert(msg_bind_req == msg->header.type);

    rsp_msg.has_mapped_addr = 1;
    vsockaddr_to_addrv4(from, &rsp_msg.mapped_addr);

    memset(&rsp_msg, 0, sizeof(rsp_msg));
    rsp_msg.header.type  = msg_bind_rsp;
    rsp_msg.header.len   = -1;
    rsp_msg.header.magic = STUN_MAGIC;
    memcpy(rsp_msg.header.trans_id, msg->header.trans_id, 12);

    ret =_aux_snd_msg(stun, &rsp_msg, from);
    retE((ret < 0));
    return 0;
}

static
int _vstun_rcv_rsp_msg(struct vstun* stun, struct vstun_msg* msg, struct sockaddr_in* from)
{
    struct vstun_req_item* req = NULL;
    struct vlist* node = NULL;
    struct sockaddr_in addr;

    vassert(stun);
    vassert(msg);
    vassert(from);
    vassert(msg_bind_rsp == msg->header.type);

    retE((!msg->has_mapped_addr));
    vsockaddr_from_addrv4(&msg->mapped_addr, &addr);

    vlock_enter(&stun->lock);
    __vlist_for_each(node, &stun->reqs) {
        req = vlist_entry(node, struct vstun_req_item, list);
        if (!memcmp(req->trans_id, msg->header.trans_id, 12)) {
            vlist_del(&req->list);
            break;
        }
        req = NULL;
    }
    vlock_leave(&stun->lock);
    req->cb(&addr, req->cookie);
    return 0;
}

static
void _vstun_reap_timeout_reqs(struct vstun* stun)
{
    struct vstun_req_item* req = NULL;
    struct vlist* node = NULL;
    time_t now = time(NULL);
    vassert(stun);

    vlock_enter(&stun->lock);
    __vlist_for_each(node, &stun->reqs) {
        req = vlist_entry(node, struct vstun_req_item, list);
        if (now - req->snd_ts > stun->max_tmo) {
            vlist_del(&req->list);
            vstun_req_free(req);
        }
    }
    vlock_leave(&stun->lock);
    return ;
}

static
void _vstun_clear(struct vstun* stun)
{
    struct vstun_req_item* req = NULL;
    struct vlist* node = NULL;

    vassert(stun);

    vlock_enter(&stun->lock);
    while(!vlist_is_empty(&stun->reqs)) {
        node = vlist_pop_head(&stun->reqs);
        req  = vlist_entry(node, struct vstun_req_item, list);
        vlist_del(&req->list);
        vstun_req_free(req);
    }
    vlock_leave(&stun->lock);
    return;
}

static
int _vstun_load_servers(struct vstun* stun)
{
    vassert(stun);

    //todo;
    return 0;
}

static
struct vstun_base_ops stun_base_ops = {
    .snd_req_msg   = _vstun_snd_req_msg,
    .rcv_req_msg   = _vstun_rcv_req_msg,
    .rcv_rsp_msg   = _vstun_rcv_rsp_msg,
    .clear         = _vstun_clear,
    .reap_timeout_reqs = _vstun_reap_timeout_reqs,
    .load_servers  = _vstun_load_servers

};

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
int _vstun_get_ext_addr(struct vstun* stun, get_ext_addr_t cb, void* cookie, struct sockaddr_in* stuns_addr)
{
    struct vstun_msg msg;
    struct vhashgen hashgen;
    vsrvcId srvId;
    int ret = 0;

    vassert(stun);

    ret = vhashgen_init(&hashgen);
    retE((ret < 0));
    ret = hashgen.ext_ops->get_stun_hash(&hashgen, &srvId);
    vhashgen_deinit(&hashgen);
    retE((ret < 0));

    memset(&msg, 0, sizeof(msg));
    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    _aux_trans_id_make(msg.header.trans_id);
    ret = stun->base_ops->snd_req_msg(stun, &msg, stuns_addr, cb, cookie);
    retE((ret < 0));
    return 0;
}


static
int _vstun_register_service(struct vstun* stun)
{
    struct vnode* node = stun->node;
    struct vhashgen hashgen;
    vsrvcId srvId;
    int ret = 0;

    vassert(stun);

    ret = vhashgen_init(&hashgen);
    retE((ret < 0));
    ret = hashgen.ext_ops->get_stun_hash(&hashgen, &srvId);
    vhashgen_deinit(&hashgen);
    retE((ret < 0));

    ret = node->ops->reg_service(node, &srvId, &stun->stun_addr);
    retE((ret < 0));
    return 0;
}

static
int _vstun_unregister_service(struct vstun* stun)
{
    struct vnode* node = stun->node;
    struct vhashgen hashgen;
    vsrvcId srvId;
    int ret = 0;

    vassert(stun);

    ret = vhashgen_init(&hashgen);
    retE((ret < 0));
    ret = hashgen.ext_ops->get_stun_hash(&hashgen, &srvId);
    vhashgen_deinit(&hashgen);
    retE((ret < 0));

    node->ops->unreg_service(node, &srvId, &stun->stun_addr);
    return 0;
}

struct vstun_ops stun_ops = {
    .get_ext_addr  = _vstun_get_ext_addr,
    .reg_service   = _vstun_register_service,
    .unreg_service = _vstun_unregister_service
};

static
int _aux_stun_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstun* stun = (struct vstun*)cookie;
    struct vstun_msg msg;
    int ret = 0;

    vassert(stun);
    vassert(mu);

    memset(&msg, 0, sizeof(msg));
    ret = stun->proto_ops->decode(mu->data, mu->len, &msg);
    retE((ret < 0));

    switch(msg.header.type) {
    case msg_bind_req:
        ret = stun->base_ops->rcv_req_msg(stun, &msg, to_sockaddr_sin(mu->addr));
        retE((ret < 0));
        break;
    case msg_bind_rsp:
        ret = stun->base_ops->rcv_rsp_msg(stun, &msg, to_sockaddr_sin(mu->addr));
        retE((ret < 0));
        break;
    default:
        retE((1));
        break;
    }
    return 0;
}

int vstun_init(struct vstun* stun, struct vmsger* msger, struct vnode* node, struct sockaddr_in* addr)
{
    vassert(stun);
    vassert(msger);


    vlist_init(&stun->reqs);
    vlock_init(&stun->lock);

    vsockaddr_copy(&stun->stun_addr, addr);
    stun->msger     = msger;
    stun->node      = node;
    stun->ops       = &stun_ops;
    stun->base_ops  = &stun_base_ops;
    stun->proto_ops = &stun_proto_ops;

    msger->ops->add_cb(msger, stun, _aux_stun_msg_cb, VMSG_STUN);

    return 0;
}

void vstun_deinit(struct vstun* stun)
{
    vassert(stun);

    stun->base_ops->clear(stun);
    vlock_deinit(&stun->lock);
    return ;
}

