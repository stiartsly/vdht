#include "vglobal.h"
#include "vstun.h"

extern struct vstun_msg_ops stun_msg_ops;

static
int _vstun_render_service(struct vstun* stun)
{
    struct vhost* host = stun->host;
    int ret = 0;
    vassert(stun);

    ret = host->ops->plug(host, PLUGIN_STUN, &stun->my_addr);
    retE((ret < 0));
    vlogI(printf("stun service registered"));
    return 0;
}

static
int _vstun_unrender_service(struct vstun * stun)
{
    struct vhost* host = stun->host;
    int ret = 0;
    vassert(stun);

    ret = host->ops->unplug(host, PLUGIN_STUN, &stun->my_addr);
    retE((ret < 0));
    vlogI(printf("stun service unregistered"));
    return 0;
}

static
int _aux_stun_thread_entry(void* argv)
{
    struct vstun* stun = (struct vstun*)argv;
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
int _vstun_daemonize(struct vstun* stun)
{
    int ret = 0;
    vassert(stun);

    stun->daemonized = 0;
    stun->to_quit    = 0;

    ret = vthread_init(&stun->daemon, _aux_stun_thread_entry, stun);
    retE((ret < 0));
    vthread_start(&stun->daemon);
    stun->daemonized = 1;

    return 0;
}

static
int _vstun_stop(struct vstun* stun)
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

struct vstun_core_ops stun_ops = {
    .render    = _vstun_render_service,
    .unrender  = _vstun_unrender_service,
    .daemonize = _vstun_daemonize,
    .stop      = _vstun_stop
};

static
int _aux_stun_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstun* stun = (struct vstun*)cookie;
    struct vstun_msg req;
    struct vstun_msg rsp;
    void* argv[4] = {NULL,};
    void* buf = NULL;
    int ret = 0;

    vassert(stun);
    vassert(mu);

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));

    ret = stun->msg_ops->decode(stun, mu->data, mu->len, &req);
    retE((ret < 0));

    argv[0] = &rsp;
    argv[1] = to_sockaddr_sin(mu->addr);
    argv[2] = stun->my_name;
    ret = stun->msg_ops->handle(stun, &req, argv);
    retE((ret < 0));
    retS((ret > 0)); // means success (or error) response.
                     // do not need response message.

    buf = malloc(BUF_SZ);
    vlog((!buf), elog_malloc);
    retE((!buf));

    ret = stun->msg_ops->encode(stun, &rsp, buf, BUF_SZ);
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
int _aux_stun_pack_msg_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys* sm)
{
    vassert(cookie);
    vassert(um);
    vassert(sm);
    retE((um->msgId != VMSG_STUN));

    vmsg_sys_init(sm, um->addr, um->len, um->data);
    return 0;
}

static
int _aux_stun_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    vassert(cookie);
    vassert(sm);
    vassert(um);

    vmsg_usr_init(um, VMSG_STUN, &sm->addr, sm->len, sm->data);
    return 0;
}

static
int _aux_stun_get_addr_and_name(struct vstun* stun, struct vconfig* cfg)
{
    char buf[64];
    int port = 0;
    int ret  = 0;
    vassert(cfg);

    memset(buf, 0, 64);
    ret = vhostaddr_get_first(buf, 64);
    vlog((ret < 0), elog_vhostaddr_get_first);
    retE((ret < 0));
    ret = cfg->inst_ops->get_stun_port(cfg, &port);
    retE((ret < 0));
    ret = vsockaddr_convert(buf, port, &stun->my_addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));

    memset(buf, 0, 64);
    ret = cfg->inst_ops->get_stun_server_name(cfg, stun->my_name, 64);
    retE((ret < 0));

    return 0;
}

struct vstun* vstun_create(struct vhost* host, struct vconfig* cfg)
{
    struct vstun* stun = NULL;
    struct sockaddr_in addr;
    int ret = 0;

    vassert(host);
    vassert(cfg);

    stun = (struct vstun*)malloc(sizeof(*stun));
    vlog((!stun), elog_malloc);
    retE_p((!stun));
    memset(stun, 0, sizeof(*stun));

    ret = _aux_stun_get_addr_and_name(stun, cfg);
    ret1E_p((ret < 0), free(stun));

    stun->host    = host;
    stun->msg_ops = &stun_msg_ops;
    stun->ops     = &stun_ops;

    ret += vmsger_init (&stun->msger);
    ret += vrpc_init   (&stun->rpc, &stun->msger, VRPC_UDP, to_vsockaddr_from_sin(&addr));
    ret += vwaiter_init(&stun->waiter);
    if (ret < 0) {
        vwaiter_deinit (&stun->waiter);
        vrpc_deinit    (&stun->rpc);
        vmsger_deinit  (&stun->msger);
        free(stun);
        return NULL;
    }
    stun->waiter.ops->add(&stun->waiter, &stun->rpc);
    vmsger_reg_pack_cb  (&stun->msger, _aux_stun_pack_msg_cb,   stun);
    vmsger_reg_unpack_cb(&stun->msger, _aux_stun_unpack_msg_cb, stun);
    stun->msger.ops->add_cb(&stun->msger, stun, _aux_stun_msg_cb, VMSG_STUN);

    return stun;
}

void vstun_destroy(struct vstun* stun)
{
    vassert(stun);

    stun->ops->stop(stun);
    stun->waiter.ops->remove(&stun->waiter, &stun->rpc);
    vwaiter_deinit(&stun->waiter);
    vrpc_deinit   (&stun->rpc);
    vmsger_deinit (&stun->msger);

    return ;
}

