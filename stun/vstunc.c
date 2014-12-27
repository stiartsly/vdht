#include "vglobal.h"
#include "vstun_proto.h"
#include "vstunc.h"

extern struct vstun_proto_ops stun_proto_ops;
struct vstunc_req {
    struct vlist list;
    uint8_t trans_id[12];
    stunc_msg_cb_t cb;
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
void vstunc_req_init(struct vstunc_req* req, uint8_t* trans_id, stunc_msg_cb_t cb, void* cookie)
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
    time_t used_ts;
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
void vstunc_srv_init(struct vstunc_srv* srv, struct sockaddr_in* addr)
{
    vassert(srv);

    vlist_init(&srv->list);
    vsockaddr_copy(&srv->addr, addr);
    srv->used_ts = 0;

    return;
}

static
int _vstunc_add_server(struct vstunc* stunc, struct sockaddr_in* srv_addr)
{
    struct vstunc_srv* srv = NULL;

    vassert(stunc);
    vassert(srv_addr);

    srv = vstunc_srv_alloc();
    vlog((!srv), elog_vstunc_srv_alloc);
    retE((!srv));
    vstunc_srv_init(srv, srv_addr);

    vlock_enter(&stunc->lock);
    vlist_add_tail(&stunc->servers, &srv->list);
    vlock_leave(&stunc->lock);

    return 0;
}

static
int _vstunc_get_server(struct vstunc* stunc, struct sockaddr_in* srv_addr)
{
    vassert(stunc);
    vassert(srv_addr);

    //todo;
    return 0;
}

static
int _vstunc_snd_req_msg(struct vstunc* stunc, struct vstun_msg* msg, struct sockaddr_in* to, stunc_msg_cb_t cb, void* cookie)
{
    struct vstunc_req* req = NULL;
    void* buf = NULL;
    int ret = 0;

    vassert(stunc);
    vassert(msg);
    vassert(to);

    buf = malloc(BUF_SZ);
    vlog((!buf), elog_malloc);
    retE((!buf));

    ret = stunc->proto_ops->encode(msg, buf, BUF_SZ);
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

    vstunc_req_init(req, msg->header.trans_id, cb, cookie);
    vlock_enter(&stunc->lock);
    vlist_add_tail(&stunc->items, &req->list);
    vlock_leave(&stunc->lock);

    return 0;
}

static
int _vstunc_rcv_rsp_msg(struct vstunc* stunc, struct vstun_msg* msg, struct sockaddr_in* from)
{
    struct vstunc_req* req = NULL;
    struct vlist* node = NULL;

    vassert(stunc);
    vassert(msg);
    vassert(from);

    vlock_enter(&stunc->lock);
    __vlist_for_each(node, &stunc->items) {
        req = vlist_entry(node, struct vstunc_req, list);
        if (!memcmp(req->trans_id, msg->header.trans_id, 12)) {
            vlist_del(&req->list);
            break;
        }
        req = NULL;
    }
    vlock_leave(&stunc->lock);
    retS((!req)); // skip the message with wrong trans_id

    if (msg->header.type != msg_bind_rsp) {
        vstunc_req_free(req);
        return 0;
    }

    req->cb(req->cookie, msg);
    vstunc_req_free(req);
    return 0;
}

static
void _vstunc_clear(struct vstunc* stunc)
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
    return;
}

static
void _vstunc_dump(struct vstunc* stunc)
{
    vassert(stunc);

    //todo;
    return;
}

static
struct vstunc_ops stunc_ops = {
    .add_srv     = _vstunc_add_server,
    .get_srv     = _vstunc_get_server,
    .snd_req_msg = _vstunc_snd_req_msg,
    .rcv_rsp_msg = _vstunc_rcv_rsp_msg,
    .clear       = _vstunc_clear,
    .dump        = _vstunc_dump
};

struct vstunc_nat_params {
    uint32_t which_step;
    struct sockaddr_in my_last_addr;
};

static
struct vstunc_nat_params stunc_nat_params = {
    .which_step = NAT_CHECK_STEP0
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

/*
 * the routine to check whether network is fully-blocked by NAT.
 *
 * @srv:
 */
static
int _vstunc_check_step1(struct vstunc* stunc, stunc_msg_cb_t cb, void* cookie)
{
    struct vstun_msg   msg;
    struct sockaddr_in to;
    int   ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->ops->get_srv(stunc, &to);
    retE((ret < 0));
    ret = stunc->ops->snd_req_msg(stunc, &msg, &to, cb, cookie);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to check whether network is full-cone NAT.
 */
static
int _vstunc_check_step2(struct vstunc* stunc, stunc_msg_cb_t cb, void* cookie)
{
    struct vstun_msg  msg;
    struct sockaddr_in to;
    int ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    //todo; req_change attribute needed.
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->ops->get_srv(stunc, &to);
    retE((ret < 0));
    ret = stunc->ops->snd_req_msg(stunc, &msg, &to, cb, cookie);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to check whether network is symmetric NAT.
 */
static
int _vstunc_check_step3(struct vstunc* stunc, stunc_msg_cb_t cb, void* cookie)
{
    struct vstun_msg  msg;
    struct sockaddr_in to;
    int ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    //todo; req_change attribute needed.
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->ops->get_srv(stunc, &to);
    retE((ret < 0));
    ret = stunc->ops->snd_req_msg(stunc, &msg, &to, cb, cookie);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to check whether network is restricted cone NAT or port restricted
 * cone NAT.
 */
static
int _vstunc_check_step4(struct vstunc* stunc, stunc_msg_cb_t cb, void* cookie)
{
    struct vstun_msg  msg;
    struct sockaddr_in to;
    int ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    //todo; req_change attribute needed.
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->ops->get_srv(stunc, &to);
    retE((ret < 0));
    ret = stunc->ops->snd_req_msg(stunc, &msg, &to, cb, cookie);
    retE((ret < 0));
    return 0;
}

static
int _aux_check_nat_type_cb(void* cookie, struct vstun_msg* msg)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstunc_nat_params* params = (struct vstunc_nat_params*)stunc->params;
    int ret = 0;
    vassert(stunc);
    vassert(msg);

    switch(params->which_step) {
    case NAT_CHECK_STEP0:
        //deal message.
        params->which_step = NAT_CHECK_STEP1;
        ret = stunc->nat_ops->check_step2(stunc, _aux_check_nat_type_cb, stunc);
        break;

    case NAT_CHECK_STEP1:
        //deal message;
        params->which_step = NAT_CHECK_STEP2;
        ret = stunc->nat_ops->check_step3(stunc, _aux_check_nat_type_cb, stunc);
        break;
    case NAT_CHECK_STEP2:
        //deal message;
        params->which_step = NAT_CHECK_STEP3;
        ret = stunc->nat_ops->check_step3(stunc, _aux_check_nat_type_cb, stunc);
        break;
    case NAT_CHECK_STEP3:
        //deal message;
        params->which_step = NAT_CHECK_STEP3_NEXT;
        ret = stunc->nat_ops->check_step3(stunc, _aux_check_nat_type_cb, stunc);
        break;
    case NAT_CHECK_STEP3_NEXT:
        //todo:
        params->which_step = NAT_CHECK_STEP4;
        ret = stunc->nat_ops->check_step4(stunc, _aux_check_nat_type_cb, stunc);
        break;
    case NAT_CHECK_STEP4:
        //todo;
        params->which_step = NAT_CHECK_SUCC;
        //todo;
        break;
    case NAT_CHECK_ERR:
    case NAT_CHECK_SUCC:
    case NAT_CHECK_TIMEOUT:
        //todo;
        break;
    default:
        vassert(0);
    }
    if (ret < 0) {
        params->which_step = NAT_CHECK_ERR;
    }
    return 0;
}

static
int _vstunc_check_nat_type(struct vstunc* stunc)
{
    struct vstunc_nat_params* params = (struct vstunc_nat_params*)stunc->params;
    int ret = 0;
    vassert(stunc);

    memset(params, 0, sizeof(*params));
    params->which_step = NAT_CHECK_STEP0;

    ret = stunc->nat_ops->check_step1(stunc, _aux_check_nat_type_cb, stunc);
    retE((ret < 0));
    return 0;
}

static
struct vstunc_nat_ops stunc_nat_ops = {
    .check_step1 = _vstunc_check_step1,
    .check_step2 = _vstunc_check_step2,
    .check_step3 = _vstunc_check_step3,
    .check_step3 = _vstunc_check_step4,
    .check_nat_type = _vstunc_check_nat_type
};

static
int _aux_stunc_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstun_msg msg;
    int ret = 0;

    vassert(stunc);
    vassert(mu);

    memset(&msg, 0, sizeof(msg));
    ret = stunc->proto_ops->decode(mu->data, mu->len, &msg);
    retE((ret < 0));

    ret = stunc->ops->rcv_rsp_msg(stunc, &msg, to_sockaddr_sin(mu->addr));
    retE((ret < 0));
    return 0;
}

int vstunc_init(struct vstunc* stunc, struct vmsger* msger)
{
    vassert(stunc);

    vlist_init(&stunc->items);
    vlock_init(&stunc->lock);

    stunc->msger     = msger;
    stunc->params    = &stunc_nat_params;
    stunc->ops       = &stunc_ops;
    stunc->nat_ops   = &stunc_nat_ops;
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

