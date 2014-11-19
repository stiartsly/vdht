#include "vglobal.h"
#include "vlsctl.h"

/* message interface for local service controller.*/
#define IS_LSCTL_MAGIC(val) (val == (uint32_t)0x7fec45fa)

static
int _aux_get_addr(void* data, int offset, struct sockaddr_in* addr)
{
    char ip[64];
    int port = 0;
    int ret = 0;
    int sz = 0;

    vassert(data);
    vassert(offset > 0);
    vassert(addr);

    port = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);

    memset(ip, 0, 64);
    strcpy(ip, (char*)offset_addr(data, offset + sz));
    sz += strlen(ip) + 1;

    ret = vsockaddr_convert(ip, port, addr);
    retE((ret < 0));
    return sz;
}

static
int _vlsctl_host_up(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    int ret = 0;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = host->ops->start(host);
    retE((ret < 0));
    return 0;
}

static
int _vlsctl_host_down(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    int ret = 0;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = host->ops->stop(host);
    retE((ret < 0));
    return 0;
}

static
int _vlsctl_host_exit(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    int ret = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = host->ops->req_quit(host);
    retE((ret < 0));
    return 0;
}

static
int _vlsctl_host_dump(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    host->ops->dump(host);
    return 0;
}

static
int _vlsctl_dht_query(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int qId  = 0;
    int ret  = 0;
    int sz   = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    qId = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);
    vlogI(printf("[vlsctl] reqeust to send @%s query", vdht_get_desc(qId)));

    ret = _aux_get_addr(data, offset + sz, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->bogus_query(host, qId, &sin);
    retE((ret < 0));
    return sz;
}

static
int _vlsctl_add_node(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int ret  = 0;
    int sz = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = _aux_get_addr(data, offset, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->join(host, &sin);
    retE((ret < 0));

    vlogI(printf("[vlsctl] host joined a node"));
    return sz;
}

static
int _vlsctl_del_node(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int ret  = 0;
    int sz   = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = _aux_get_addr(data, offset, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->drop(host, &sin);
    retE((ret < 0));

    vlogI(printf("[vlsctl] host dropped a node"));
    return sz;
}

static
int _vlsctl_service_announce(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int what = 0;
    int ret = 0;
    int sz = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    what = get_int32(offset_addr(data, offset));
    retE((what <  PLUGIN_RELAY));
    retE((what >= PLUGIN_BUTT));
    sz += sizeof(int32_t);

    ret = _aux_get_addr(data, offset + sz, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->plug(host, what, &sin);
    retE((ret < 0));
    vlogI(printf("service (%s) published", vpluger_get_desc(what)));
    return sz;
}

static
int _vlsctl_service_unavailable(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int what = 0;
    int ret = 0;
    int sz  = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    what = get_int32(offset_addr(data, offset + sz));
    retE((what <  PLUGIN_RELAY));
    retE((what >= PLUGIN_BUTT ));
    sz += sizeof(int32_t);

    ret = _aux_get_addr(data, offset + sz, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->unplug(host, what, &sin);
    retE((ret < 0));
    vlogI(printf("service (%s) unavailable.", vpluger_get_desc(what)));
    return sz;
}

static
int _vlsctl_service_pick(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in addr;
    int what = 0;
    int ret = 0;
    int sz  = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    what = get_int32(offset_addr(data, offset));
    retE((what <  PLUGIN_RELAY));
    retE((what >= PLUGIN_BUTT ));
    sz += sizeof(int32_t);

    ret = host->ops->req_plugin(host, what, &addr);
    retE((ret < 0));
    vsockaddr_dump(&addr);
    return sz;
}

static
int _vlsctl_cfg_dump(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    host->cfg->ops->dump(host->cfg);
    return 0;
}

static
int _vlsctl_dispatch_msg(struct vlsctl* lsctl, void* data, int offset)
{
    int (*routine_cb[])(struct vlsctl*, void*, int) = {
        lsctl->ops->host_up,
        lsctl->ops->host_down,
        lsctl->ops->host_exit,
        lsctl->ops->host_dump,
        lsctl->ops->dht_query,
        lsctl->ops->add_node,
        lsctl->ops->del_node,
        lsctl->ops->service_announce,
        lsctl->ops->service_unavailable,
        lsctl->ops->service_pick,
        lsctl->ops->cfg_dump,
        NULL
    };
    int what = 0;
    int ret = 0;
    int sz = 0;

    vassert(lsctl);
    vassert(data);
    vassert(!offset);

    what = get_int32(data);
    while(!IS_LSCTL_MAGIC((uint32_t)what)) {
        sz += sizeof(int32_t);
        retE((what < 0));
        retE((what >= VLSCTL_BUTT));

        ret = routine_cb[what](lsctl, data, sz);
        retE((ret < 0));
        sz += ret;

        what = get_int32(offset_addr(data, sz));

    }
    return 0;
}

static
struct vlsctl_ops lsctl_ops = {
    .host_up    = _vlsctl_host_up,
    .host_down  = _vlsctl_host_down,
    .host_exit  = _vlsctl_host_exit,
    .host_dump  = _vlsctl_host_dump,
    .dht_query  = _vlsctl_dht_query,
    .add_node   = _vlsctl_add_node,
    .del_node   = _vlsctl_del_node,
    .service_announce    = _vlsctl_service_announce,
    .service_unavailable = _vlsctl_service_unavailable,
    .service_pick        = _vlsctl_service_pick,
    .cfg_dump   = _vlsctl_cfg_dump,
    .dsptch_msg = _vlsctl_dispatch_msg
};

int _aux_vlsctl_msg_invoke_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* lsctl = (struct vlsctl*)cookie;
    int ret = 0;

    ret = lsctl->ops->dsptch_msg(lsctl, um->data, 0);
    retE((ret < 0));
    return 0;
}

static
int _aux_vlsctl_msg_unpack_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    uint32_t magic = 0;
    void* data = sm->data;
    int  msgId = 0;
    int  sz = 0;

    vassert(sm);
    vassert(um);

    magic = get_uint32(data);
    retE((!IS_LSCTL_MAGIC(magic)));
    data = offset_addr(data, sizeof(uint32_t));
    sz += sizeof(uint32_t);

    msgId = get_int32(data);
    retE((msgId != VMSG_LSCTL));
    data = offset_addr(data, sizeof(int32_t));
    sz += sizeof(int32_t);

    vmsg_usr_init(um, msgId, &sm->addr, sm->len-sz, data);
    return 0;
}

int vlsctl_init(struct vlsctl* ctl, struct vhost* host)
{
    struct vconfig_ops* ops = host->cfg->ops;
    struct sockaddr_un* sun = &ctl->addr.vsun_addr;
    char unix_path[BUF_SZ];
    int ret = 0;

    vassert(ctl);
    vassert(host);

    memset(unix_path, 0, BUF_SZ);
    ret = ops->get_str_ext(host->cfg, "lsctl.unix_path", unix_path, BUF_SZ, DEF_LSCTL_UNIX_PATH);
    retE((ret < 0));
    retE((strlen(unix_path) + 1 >= sizeof(sun->sun_path)));

    sun->sun_family = AF_UNIX;
    strcpy(sun->sun_path, unix_path);
    ctl->host = host;
    ctl->ops  = &lsctl_ops;

    ret += vmsger_init(&ctl->msger);
    ret += vrpc_init(&ctl->rpc, &ctl->msger, VRPC_UNIX, &ctl->addr);
    if (ret < 0) {
        vrpc_deinit(&ctl->rpc);
        vmsger_deinit(&ctl->msger);
        return -1;
    }

    vmsger_reg_unpack_cb  (&ctl->msger, _aux_vlsctl_msg_unpack_cb, ctl);
    ctl->msger.ops->add_cb(&ctl->msger, ctl, _aux_vlsctl_msg_invoke_cb, VMSG_LSCTL);
    return 0;
}

void vlsctl_deinit(struct vlsctl* ctl)
{
    vassert(ctl);

    vmsger_deinit(&ctl->msger);
    vrpc_deinit(&ctl->rpc);
    return ;
}

