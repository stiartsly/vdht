#include "vglobal.h"
#include "vstun_proto.h"
#include "vstuns.h"

extern struct vstun_proto_ops stun_proto_ops;

static
int _vstuns_render_service(struct vstuns* stun)
{
    struct vhashgen* hashgen = stun->hashgen;
    struct vhost* host = stun->host;
    vtoken hash;
    int ret = 0;
    vassert(stun);

    ret = hashgen->inst_ops->get_stun_hash(hashgen, &hash);
    retE((ret < 0));
    ret = host->ops->plug_service(host, &hash, &stun->my_addr);
    retE((ret < 0));
    vlogI(printf("stun service registered"));
    return 0;
}

static
int _vstuns_unrender_service(struct vstuns * stun)
{
    struct vhashgen* hashgen = stun->hashgen;
    struct vhost* host = stun->host;
    vtoken hash;
    int ret = 0;
    vassert(stun);

    ret = hashgen->inst_ops->get_stun_hash(hashgen, &hash);
    retE((ret < 0));
    ret = host->ops->unplug_service(host, &hash, &stun->my_addr);
    retE((ret < 0));
    vlogI(printf("stun service unregistered"));
    return 0;
}

static
int _vstuns_parse_msg(struct vstuns* stun, void* argv)
{
    varg_decl(argv, 0, struct sockaddr_in*, from);
    varg_decl(argv, 1, struct vstun_msg*, req);
    varg_decl(argv, 2, struct vstun_msg*, rsp);
    int sz = 0;

    vassert(stun);
    vassert(req);
    vassert(from);
    vassert(rsp);

    memset(rsp, 0, sizeof(*rsp));

    rsp->has_mapped_addr = 1;
    vsockaddr_to_addrv4(from, &rsp->mapped_addr);
    sz += sizeof(struct vattr_header);
    sz += sizeof(struct vattr_addrv4);

    rsp->header.type  = msg_bind_rsp;
    rsp->header.len   = sz;
    rsp->header.magic = STUN_MAGIC;
    memcpy(rsp->header.trans_id, req->header.trans_id, 12);

    return 0;
}

static
int _aux_stuns_thread_entry(void* argv)
{
    struct vstuns*  stun   = (struct vstuns*)argv;
    struct vwaiter* waiter = &stun->waiter;
    int ret = 0;

    vassert(stun);
    vassert(waiter);

    vlogI(printf("stun service started"));
    while(!stun->to_quit) {
        ret = waiter->ops->laundry(waiter);
        if (ret < 0) {
            continue;
        }
    }
    vlogI(printf("stun service stopped"));
    return 0;
}

static
int _vstuns_daemonize(struct vstuns* stun)
{
    int ret = 0;
    vassert(stun);

    stun->daemonized = 0;
    stun->to_quit    = 0;

    ret = vthread_init(&stun->daemon, _aux_stuns_thread_entry, stun);
    retE((ret < 0));
    vthread_start(&stun->daemon);
    stun->daemonized = 1;

    return 0;
}

static
int _vstuns_stop(struct vstuns* stun)
{
    int ret = 0;
    vassert(stun);

    if (stun->daemonized) {
        int ret_code = 0;

        stun->to_quit = 1;
        ret = vthread_join(&stun->daemon, &ret_code);
        retE((ret < 0));
    }
    return 0;
}

struct vstuns_ops stuns_ops = {
    .render    = _vstuns_render_service,
    .unrender  = _vstuns_unrender_service,
    .parse_msg = _vstuns_parse_msg,
    .daemonize = _vstuns_daemonize,
    .stop      = _vstuns_stop
};

static
int _aux_stuns_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstuns* stun = (struct vstuns*)cookie;
    struct vstun_msg req;
    struct vstun_msg rsp;
    void* buf = NULL;
    int ret = 0;

    vassert(stun);
    vassert(mu);

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));

    ret = stun->proto_ops->decode(mu->data, mu->len, &req);
    retE((ret < 0));
    retE((req.header.type != msg_bind_req)); //skip non-request message.

    {
        void* argv[] = {
            to_sockaddr_sin(mu->addr),
            &req,
            &rsp
        };
        ret = stun->ops->parse_msg(stun, argv);
        retE((ret < 0));
    }

    buf = malloc(BUF_SZ);
    vlog((!buf), elog_malloc);
    retE((!buf));

    ret = stun->proto_ops->encode(&rsp, buf, BUF_SZ);
    ret1E((ret < 0), free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = mu->addr,
            .msgId = VMSG_STUN,
            .data  = buf,
            .len   = ret
        };
        ret = stun->msger.ops->push(&stun->msger, &msg);
        ret1E((ret < 0), free(buf));
    }
    return 0;
}

static
int _aux_stuns_pack_msg_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys* sm)
{
    vassert(cookie);
    vassert(um);
    vassert(sm);
    retE((um->msgId != VMSG_STUN));

    vmsg_sys_init(sm, um->addr, um->len, um->data);
    return 0;
}

static
int _aux_stuns_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    vassert(cookie);
    vassert(sm);
    vassert(um);

    vmsg_usr_init(um, VMSG_STUN, &sm->addr, sm->len, sm->data);
    return 0;
}

static
int _aux_stuns_get_addr(struct vconfig* cfg, struct sockaddr_in* addr)
{
    char buf[64];
    int port = 0;
    int ret  = 0;

    vassert(cfg);
    vassert(addr);

    memset(buf, 0, 64);
    ret = vhostaddr_get_first(buf, 64);
    vlog((ret < 0), elog_vhostaddr_get_first);
    retE((ret < 0));
    ret = cfg->inst_ops->get_stun_port(cfg, &port);
    retE((ret < 0));
    ret = vsockaddr_convert(buf, port, addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));

    return 0;
}

struct vstuns* vstuns_create(struct vhost* host, struct vconfig* cfg)
{
    struct vstuns* stun = NULL;
    int ret = 0;

    vassert(host);
    vassert(cfg);

    stun = (struct vstuns*)malloc(sizeof(*stun));
    vlog((!stun), elog_malloc);
    retE_p((!stun));
    memset(stun, 0, sizeof(*stun));

    ret = _aux_stuns_get_addr(cfg, &stun->my_addr);
    ret1E_p((ret < 0), free(stun));
    stun->host      = host;
    stun->proto_ops = &stun_proto_ops;
    stun->ops       = &stuns_ops;
    stun->hashgen   = NULL; //host->hashgen;
    ret += vmsger_init (&stun->msger);
    ret += vrpc_init   (&stun->rpc, &stun->msger, VRPC_UDP, to_vsockaddr_from_sin(&stun->my_addr));
    ret += vwaiter_init(&stun->waiter);
    if (ret < 0) {
        vwaiter_deinit (&stun->waiter);
        vrpc_deinit    (&stun->rpc);
        vmsger_deinit  (&stun->msger);
        free(stun);
        return NULL;
    }
    stun->waiter.ops->add(&stun->waiter, &stun->rpc);
    vmsger_reg_pack_cb  (&stun->msger, _aux_stuns_pack_msg_cb,   stun);
    vmsger_reg_unpack_cb(&stun->msger, _aux_stuns_unpack_msg_cb, stun);
    stun->msger.ops->add_cb(&stun->msger, stun, _aux_stuns_msg_cb, VMSG_STUN);

    return stun;
}

void vstuns_destroy(struct vstuns* stun)
{
    vassert(stun);

    stun->ops->stop(stun);
    stun->waiter.ops->remove(&stun->waiter, &stun->rpc);
    vwaiter_deinit(&stun->waiter);
    vrpc_deinit   (&stun->rpc);
    vmsger_deinit (&stun->msger);

    return ;
}

