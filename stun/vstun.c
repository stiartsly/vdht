#include "vglobal.h"
#include "vstun.h"

extern struct vstun_msg_ops stun_msg_ops;

static
int _vstun_render_service(struct vstun* stun)
{
    struct vattr_addrv4* source = &stun->cache.source;
    struct vhost* host = stun->host;
    struct sockaddr_in addr;
    int ret = 0;
    vassert(stun);

    vsockaddr_convert2(source->addr, source->port, &addr);
    ret = host->ops->plug(host, PLUGIN_STUN, &addr);
    retE((ret < 0));
    return 0;
}

static
int _vstun_unrender_service(struct vstun * stun)
{
    struct vattr_addrv4* source = &stun->cache.source;
    struct vhost* host = stun->host;
    struct sockaddr_in addr;
    int ret = 0;
    vassert(stun);

    vsockaddr_convert2(source->addr, source->port, &addr);
    ret = host->ops->unplug(host, PLUGIN_STUN, &addr);
    retE((ret < 0));
    return 0;
}

static
int _vstun_daemonize(struct vstun* stun)
{
    vassert(stun);

    //todo;
    return 0;
}

static
int _vstun_stop(struct vstun* stun)
{
    vassert(stun);

    //todo;
    return 0;
}

struct vstun_core_ops stun_ops = {
    .render    = _vstun_render_service,
    .unrender  = _vstun_unrender_service,
    .daemonize = _vstun_daemonize,
    .stop      = _vstun_stop
};

static
void vattr_addrv4_init(struct vattr_addrv4* addr, struct sockaddr_in* s_addr)
{
    vassert(addr);
    vassert(s_addr);

    addr->family = family_ipv4;
    addr->pad    = 0;
    vsockaddr_unconvert2(s_addr, &addr->addr, &addr->port);
    return ;
}

static
int _aux_stun_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstun* stun = (struct vstun*)cookie;
    struct vstun_msg req;
    struct vstun_msg rsp;
    void* buf = NULL;
    int ret = 0;
    vassert(stun);
    vassert(mu);

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));

    vattr_addrv4_init(&stun->cache.from, to_sockaddr_sin(mu->addr));
    ret = stun->msg_ops->decode(stun, mu->data, mu->len, &req);
    retE((ret < 0));
    ret = stun->msg_ops->handle(stun, &req, &rsp);
    retE((ret < 0));

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
int _aux_stun_get_addr(struct vconfig* cfg, struct sockaddr_in* addr)
{
    char ip[64];
    int port = 0;
    int ret  = 0;
    vassert(cfg);

    memset(ip, 0, 64);
    ret = vhostaddr_get_first(ip, 64);
    vlog((ret < 0), elog_vhostaddr_get_first);
    retE((ret < 0));
    ret = cfg->inst_ops->get_stun_port(cfg, &port);
    retE((ret < 0));
    ret = vsockaddr_convert(ip, port, addr);
    vlog((ret < 0), elog_vsockaddr_convert);
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

    ret = _aux_stun_get_addr(cfg, &addr);
    retE_p((ret < 0));

    stun = (struct vstun*)malloc(sizeof(*stun));
    vlog((!stun), elog_malloc);
    retE_p((!stun));
    memset(stun, 0, sizeof(*stun));

    stun->cache.has_source_addr = 1;
    stun->cache.has_alt_addr    = 0;
    stun->cache.has_second_addr = 0;
    vattr_addrv4_init(&stun->cache.source, &addr);

    stun->host = host;
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

