#include "vglobal.h"
#include "vstun_proto.h"
#include "vstunc.h"

extern struct vstun_proto_ops stun_proto_ops;
struct vstunc_req {
    struct vstun_msg msg;
    struct sockaddr_in to;
    stunc_msg_cb_t cb;
    void*  cookie;
    time_t snd_ts;
    int    try_snd_tms;
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
void vstunc_req_init(struct vstunc_req* req, struct vstun_msg* msg, struct sockaddr_in* to, stunc_msg_cb_t cb, void* cookie)
{
    vassert(req);
    vassert(msg);
    vassert(cb);

    memcpy(&req->msg, msg, sizeof(*msg));
    vsockaddr_copy(&req->to, to);
    req->cb = cb;
    req->cookie = cookie;
    req->snd_ts = time(NULL);
    req->try_snd_tms = 0;

    return ;
}

static
void vstunc_req_dump(struct vstunc_req* req)
{
    vassert(req);

    //todo;
    return ;
}

struct vstunc_srv {
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

    vsockaddr_copy(&srv->addr, addr);
    srv->used_ts = 0;

    return;
}

static
void vstunc_srv_dump(struct vstunc_srv* srv)
{
    vassert(srv);

    //todo;
    return ;
}

static
int _vstunc_add_server(struct vstunc* stunc, struct sockaddr_in* srv_addr)
{
    struct vstunc_srv* srv = NULL;
    int found = 0;
    int i = 0;

    vassert(stunc);
    vassert(srv_addr);

    vlock_enter(&stunc->lock);
    for (i = 0; i < varray_size(&stunc->servers); i++) {
        srv = (struct vstunc_srv*)varray_get(&stunc->servers, i);
        if (vsockaddr_equal(&srv->addr, srv_addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        srv = vstunc_srv_alloc();
        vlog((!srv), elog_vstunc_srv_alloc);
        ret1E((!srv), vlock_leave(&stunc->lock));
        vstunc_srv_init(srv, srv_addr);
        varray_add_tail(&stunc->servers, srv);
    }
    vlock_leave(&stunc->lock);

    return 0;
}

static
int _aux_snd_req_msg(struct vstunc* stunc, struct vstun_msg* msg, struct sockaddr_in* to)
{
    void* buf = NULL;
    int ret = 0;

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
    return 0;
}

static
int _vstunc_snd_req_msg(struct vstunc* stunc, struct vstun_msg* msg, stunc_msg_cb_t cb, void* cookie, int multi_need)
{
    struct vstunc_req* req = NULL;
    struct vstunc_srv* srv = NULL;
    struct sockaddr_in srv_addr;
    int ret = 0;

    vassert(stunc);
    vassert(msg);

    vlock_enter(&stunc->lock);
    if (multi_need && (varray_size(&stunc->servers) < 2)) { //need 2 servers to work out.
        vlock_leave(&stunc->lock);
        return -1;
    }
    srv = (struct vstunc_srv*)varray_del(&stunc->servers, 0);
    vsockaddr_copy(&srv_addr, &srv->addr);
    varray_add_tail(&stunc->servers, srv);
    vlock_leave(&stunc->lock);

    ret = _aux_snd_req_msg(stunc, msg, &srv_addr);
    retE((ret < 0));

    req = vstunc_req_alloc();
    vlog((!req), elog_vstunc_req_alloc);
    retE((!req));

    vstunc_req_init(req, msg, &srv_addr, cb, cookie);
    vlock_enter(&stunc->lock);
    varray_add_tail(&stunc->items, req);
    vlock_leave(&stunc->lock);

    return 0;
}

static
int _vstunc_rcv_rsp_msg(struct vstunc* stunc, struct vstun_msg* msg, struct sockaddr_in* from)
{
    struct vstunc_req* req = NULL;
    int found = 0;
    int i = 0;

    vassert(stunc);
    vassert(msg);
    vassert(from); //unused for "from".

    vlock_enter(&stunc->lock);
    for (i = 0; i < varray_size(&stunc->items); i++) {
        req = (struct vstunc_req*)varray_get(&stunc->items, i);
        if (!memcpy(&req->msg.header.trans_id, msg->header.trans_id, 12)) {
            varray_del(&stunc->items, i);
            found = 1;
            break;
        }
    }
    vlock_leave(&stunc->lock);
    retS((!found)); // skip unrecorded mssage.
    if (msg->header.type != msg_bind_rsp) {
        vstunc_req_free(req); // skip non-success-responsed message.
        return 0;
    }
    req->cb(req->cookie, msg);
    vstunc_req_free(req);
    return 0;
}

static
int _vstunc_reap_timeout_reqs(struct vstunc* stunc)
{
    struct vstunc_req* req = NULL;
    time_t now = time(NULL);
    int i = 0;
    vassert(stunc);

    vlock_enter(&stunc->lock);
    for (i = 0; i < varray_size(&stunc->items); i++) {
        req = (struct vstunc_req*)varray_get(&stunc->items, i);
        if ((now - req->snd_ts) < stunc->max_snd_tmo) {
            continue;
        }
        if (req->try_snd_tms < stunc->max_snd_tms) {
            _aux_snd_req_msg(stunc, &req->msg, &req->to);
            req->snd_ts = now;
            req->try_snd_tms++;
        } else {
            varray_del(&stunc->items, i); // abosolete request.
            vstunc_req_free(req);
        }
    }
    vlock_leave(&stunc->lock);
    return 0;
}

static
void _vstunc_clear(struct vstunc* stunc)
{
    struct vstunc_req* req = NULL;
    struct vstunc_srv* srv = NULL;
    vassert(stunc);

    vlock_enter(&stunc->lock);
    while(varray_size(&stunc->items) > 0) {
        req = (struct vstunc_req*)varray_del(&stunc->items, 0);
        vstunc_req_free(req);
    }
    while(varray_size(&stunc->servers) > 0) {
        srv = (struct vstunc_srv*)varray_del(&stunc->servers, 0);
        vstunc_srv_free(srv);
    }
    vlock_leave(&stunc->lock);
    return;
}

static
void _vstunc_dump(struct vstunc* stunc)
{
    struct vstunc_req* req = NULL;
    struct vstunc_srv* srv = NULL;
    int i = 0;
    vassert(stunc);

    vlock_enter(&stunc->lock);
    for (i = 0; i < varray_size(&stunc->items); i++) {
        req = (struct vstunc_req*)varray_get(&stunc->items, i);
        vstunc_req_dump(req);
    }
    for (i = 0; i < varray_size(&stunc->servers); i++) {
        srv = (struct vstunc_srv*)varray_get(&stunc->servers, i);
        vstunc_srv_dump(srv);
    }
    vlock_leave(&stunc->lock);

    return ;
}

static
struct vstunc_base_ops stunc_base_ops = {
    .add_srv       = _vstunc_add_server,
    .snd_req_msg   = _vstunc_snd_req_msg,
    .rcv_rsp_msg   = _vstunc_rcv_rsp_msg,
    .reap_tmo_reqs = _vstunc_reap_timeout_reqs,
    .clear         = _vstunc_clear,
    .dump          = _vstunc_dump
};

static
int _vstunc_get_mapped_addr(struct vstunc* stunc, struct sockaddr_in* local, get_ext_addr_cb_t cb, void* cookie)
{
    vassert(stunc);
    vassert(local);
    vassert(cb);

    //todo;
    return 0;
}

struct vstunc_nat_params {
    uint32_t which_step;
    struct sockaddr_in my_last_addr;
};

static
struct vstunc_nat_params stunc_nat_params = {
    .which_step = NAT_CHECK_STEP1
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
int _vstunc_check_step1(void* cookie,  stunc_msg_cb_t cb)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstun_msg msg;
    int   ret = 0;

    vassert(stunc);
    vassert(cb);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->base_ops->snd_req_msg(stunc, &msg, cb, cookie, 0);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to check whether network is full-cone NAT.
 */
static
int _vstunc_check_step2(void* cookie, stunc_msg_cb_t cb)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstun_msg msg;
    int ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    //todo; req_change attribute needed.
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->base_ops->snd_req_msg(stunc, &msg, cb, cookie, 0);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to check whether network is symmetric NAT.
 */
static
int _vstunc_check_step3(void* cookie, stunc_msg_cb_t cb)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstun_msg msg;
    int ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    //todo; req_change attribute needed.
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->base_ops->snd_req_msg(stunc, &msg, cb, cookie, 1);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to check whether network is restricted cone NAT or port restricted
 * cone NAT.
 */
static
int _vstunc_check_step4(void* cookie, stunc_msg_cb_t cb)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstun_msg  msg;
    int ret = 0;

    vassert(stunc);

    msg.header.type  = msg_bind_req;
    msg.header.len   = 0;
    msg.header.magic = STUN_MAGIC;

    //todo; req_change attribute needed.
    _aux_trans_id_make(msg.header.trans_id);

    ret = stunc->base_ops->snd_req_msg(stunc, &msg, cb, cookie, 0);
    retE((ret < 0));
    return 0;
}

static
stunc_nat_check_t stunc_check_steps[] = {
    _vstunc_check_step1,   // when checking in step1
    _vstunc_check_step2,   // when checking in step2.
    _vstunc_check_step3,   // when checking in step3
    _vstunc_check_step3,   // when checking in bottom half of step3.
    _vstunc_check_step4,   // when checking in step4.
    0
};

static
int _aux_check_nat_type_cb(void* cookie, struct vstun_msg* msg)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vstunc_nat_params* params = (struct vstunc_nat_params*)stunc->private_params;
    int ret = 0;
    vassert(stunc);
    vassert(msg);

    switch(params->which_step) {
    case NAT_CHECK_STEP1:
        //deal message;
        params->which_step = NAT_CHECK_STEP2;
        ret = stunc->nat_ops[NAT_CHECK_STEP2](stunc, _aux_check_nat_type_cb);
        break;
    case NAT_CHECK_STEP2:
        //deal message;
        params->which_step = NAT_CHECK_STEP3;
        ret = stunc->nat_ops[NAT_CHECK_STEP3](stunc, _aux_check_nat_type_cb);
        break;
    case NAT_CHECK_STEP3:
        //deal message;
        params->which_step = NAT_CHECK_STEP3_NEXT;
        ret = stunc->nat_ops[NAT_CHECK_STEP3_NEXT](stunc, _aux_check_nat_type_cb);
        break;
    case NAT_CHECK_STEP3_NEXT:
        //todo:
        params->which_step = NAT_CHECK_STEP4;
        ret = stunc->nat_ops[NAT_CHECK_STEP4](stunc, _aux_check_nat_type_cb);
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
int _vstunc_get_nat_type(struct vstunc* stunc, get_nat_type_cb_t cb, void* cookie)
{
    struct vstunc_nat_params* params = (struct vstunc_nat_params*)stunc->private_params;
    int ret = 0;
    vassert(stunc);

    memset(params, 0, sizeof(*params));
    params->which_step = NAT_CHECK_STEP1;
    //todo;
    ret = stunc->nat_ops[NAT_CHECK_STEP1](stunc, _aux_check_nat_type_cb);
    retE((ret < 0));
    return 0;
}

static
struct vstunc_nat_ops stunc_nat_ops = {
    .get_ext_addr = _vstunc_get_mapped_addr,
    .get_nat_type = _vstunc_get_nat_type
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

    ret = stunc->base_ops->rcv_rsp_msg(stunc, &msg, to_sockaddr_sin(mu->addr));
    retE((ret < 0));
    return 0;
}

static
int _aux_stunc_tick_cb(void* cookie)
{
    struct vstunc* stunc = (struct vstunc*)cookie;
    struct vroute* route = stunc->route;
    struct sockaddr_in srv_addr;
    vtoken svc_hash;
    int ret = 0;
    vassert(stunc);

    //todo: svc_hash.
    ret = route->ops->get_service(route, &svc_hash, &srv_addr);
    if (ret) { // means service found.
        ret = stunc->base_ops->add_srv(stunc, &srv_addr);
        retE((ret < 0));
    }
    ret = stunc->base_ops->reap_tmo_reqs(stunc);
    retE((ret < 0));
    return 0;
}

int vstunc_init(struct vstunc* stunc, struct vmsger* msger, struct vticker* ticker, struct vroute* route)
{
    vassert(stunc);
    vassert(msger);
    vassert(ticker);
    vassert(route);

    varray_init(&stunc->items, 2);
    varray_init(&stunc->servers, 2);
    vlock_init(&stunc->lock);

    stunc->msger     = msger;
    stunc->ticker    = ticker;
    stunc->route     = route;
    stunc->private_params = &stunc_nat_params;
    stunc->base_ops  = &stunc_base_ops;
    stunc->nat_ops   = stunc_check_steps;
    stunc->ops       = &stunc_nat_ops;
    stunc->proto_ops = &stun_proto_ops;
    msger->ops->add_cb(msger, stunc, _aux_stunc_msg_cb, VMSG_STUN);
    ticker->ops->add_cb(ticker, _aux_stunc_tick_cb, stunc);
    return 0;
}

void vstunc_deinit(struct vstunc* stunc)
{
    vassert(stunc);

    stunc->base_ops->clear(stunc);
    vlock_deinit(&stunc->lock);

    return ;
}

