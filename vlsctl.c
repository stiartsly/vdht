#include "vglobal.h"
#include "vlsctl.h"

/* message interface for local service controller.*/
#define IS_LSCTL_MAGIC(val) (val == (uint32_t)0x7fec45fa)

static
int _aux_lsctl_get_addr(void* data, int offset, struct sockaddr_in* addr)
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

/*
 * forward the request to make host online
 */
static
int _vlsctl_cmd_host_up(struct vlsctl* lsctl, void* data, int offset)
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

/*
 *  forward the request to make host offline.
 */
static
int _vlsctl_cmd_host_down(struct vlsctl* lsctl, void* data, int offset)
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

/*
 * forward the request to make vdhtd exit.
 */
static
int _vlsctl_cmd_host_exit(struct vlsctl* lsctl, void* data, int offset)
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

/*
 * forward the reqeust to dump all infos about host, so as to debug
 */
static
int _vlsctl_cmd_dump_host(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    host->ops->dump(host);
    return 0;
}

/*
 * forward the request to send query to the node with given address.
 * this request is purely for debug.
 */
static
int _vlsctl_cmd_bogus_query(struct vlsctl* lsctl, void* data, int offset)
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
    vlogI(printf("[vlsctl] request to send query(@%s)", vdht_get_desc(qId)));

    ret = _aux_lsctl_get_addr(data, offset + sz, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->bogus_query(host, qId, &sin);
    retE((ret < 0));
    return sz;
}

/*
 * forward the request to add wellknown node or node with given address
 * into dht routing table.
 */
static
int _vlsctl_cmd_join_node(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int ret  = 0;
    int sz = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = _aux_lsctl_get_addr(data, offset, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->join(host, &sin);
    retE((ret < 0));
    return sz;
}

/*
 * forward the reqeust to remove wellknown node or node with given address
 * from dht routing table.
 */
static
int _vlsctl_cmd_drop_node(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int ret  = 0;
    int sz   = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    ret = _aux_lsctl_get_addr(data, offset, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->drop(host, &sin);
    retE((ret < 0));
    return sz;
}

/*
 *  forward to announcemaent about service information after service has been
 *  established and therefor can be provided for other nodes. The service can
 *  relay, stun, ddns, ...
 */
static
int _vlsctl_cmd_srvc_pub(struct vlsctl* lsctl, void* data, int offset)
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

    ret = _aux_lsctl_get_addr(data, offset + sz, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->plug(host, what, &sin);
    retE((ret < 0));
    vlogI(printf("service (%s) published", vpluger_get_desc(what)));
    return sz;
}

/*
 * forward to the declaration of service for being unavaiable.
 */
static
int _vlsctl_cmd_srvc_unavai(struct vlsctl* lsctl, void* data, int offset)
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

    ret = _aux_lsctl_get_addr(data, offset + sz, &sin);
    retE((ret < 0));
    sz += ret;

    ret = host->ops->unplug(host, what, &sin);
    retE((ret < 0));
    vlogI(printf("service (%s) unavailable.", vpluger_get_desc(what)));
    return sz;
}

/*
 * forward the request to get the best service option for special kind.
 */
static
int _vlsctl_cmd_srvc_prefer(struct vlsctl* lsctl, void* data, int offset)
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

    ret = host->ops->get_service(host, what, &addr);
    retE((ret < 0));
    vsockaddr_dump(&addr);
    return sz;
}

/*
 * forward the reqeust to dump config
 */
static
int _vlsctl_cmd_dump_cfg(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    host->cfg->ops->dump(host->cfg);
    return 0;
}

static vlsctl_cmd_t lsctl_cmds[] = {
    _vlsctl_cmd_host_up,
    _vlsctl_cmd_host_down,
    _vlsctl_cmd_host_exit,
    _vlsctl_cmd_dump_host,
    _vlsctl_cmd_bogus_query,
    _vlsctl_cmd_join_node,
    _vlsctl_cmd_drop_node,
    _vlsctl_cmd_srvc_pub,
    _vlsctl_cmd_srvc_unavai,
    _vlsctl_cmd_srvc_prefer,
    _vlsctl_cmd_dump_cfg,
    NULL
};

static
int _vlsctl_dispatch(struct vlsctl* lsctl, struct vmsg_usr* um)
{
    int what = 0;
    int ret  = 0;
    int sz   = 0;

    vassert(lsctl);
    vassert(um);

    what = get_int32(um->data);
    while(!IS_LSCTL_MAGIC((uint32_t)what)) {
        sz += sizeof(int32_t);
        retE((what < 0));
        retE((what >= VLSCTL_BUTT));

        ret = lsctl->cmds[what](lsctl, um->data, sz);
        retE((ret < 0));
        sz += ret;

        what = get_int32(offset_addr(um->data, sz));
    }
    return 0;
}

static
struct vlsctl_ops   lsctl_ops = {
    .dispatch = _vlsctl_dispatch
};

int _aux_lsctl_usr_msg_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* lsctl = (struct vlsctl*)cookie;
    int ret = 0;

    vassert(lsctl);
    vassert(um->data);

    ret = lsctl->ops->dispatch(lsctl, um);
    retE((ret < 0));
    return 0;
}

static
int _aux_lsctl_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    uint32_t magic = 0;
    int  msgId = 0;
    int  sz = 0;

    vassert(sm);
    vassert(um);

    magic = get_uint32(offset_addr(sm->data, sz));
    retE((!IS_LSCTL_MAGIC(magic)));
    sz += sizeof(uint32_t);

    msgId = get_int32(offset_addr(sm->data, sz));
    retE((msgId != VMSG_LSCTL));
    sz += sizeof(int32_t);

    vmsg_usr_init(um, msgId, &sm->addr, sm->len-sz, offset_addr(sm->data, sz));
    return 0;
}

int vlsctl_init(struct vlsctl* lsctl, struct vhost* host, struct vconfig* cfg)
{
    struct sockaddr_un* sun = &lsctl->addr.vsun_addr;
    int ret = 0;

    vassert(lsctl);
    vassert(host);

    sun->sun_family = AF_UNIX;
    ret = cfg->inst_ops->get_lsctl_unix_path(cfg, sun->sun_path, 105);
    retE((ret < 0));

    lsctl->host = host;
    lsctl->ops  = &lsctl_ops;
    lsctl->cmds = lsctl_cmds;

    ret += vmsger_init(&lsctl->msger);
    ret += vrpc_init(&lsctl->rpc, &lsctl->msger, VRPC_UNIX, &lsctl->addr);
    if (ret < 0) {
        vrpc_deinit(&lsctl->rpc);
        vmsger_deinit(&lsctl->msger);
        return -1;
    }

    vmsger_reg_unpack_cb(&lsctl->msger,   _aux_lsctl_unpack_msg_cb, lsctl);
    lsctl->msger.ops->add_cb(&lsctl->msger, lsctl, _aux_lsctl_usr_msg_cb, VMSG_LSCTL);
    return 0;
}

void vlsctl_deinit(struct vlsctl* lsctl)
{
    vassert(lsctl);

    vmsger_deinit(&lsctl->msger);
    vrpc_deinit(&lsctl->rpc);
    return ;
}

