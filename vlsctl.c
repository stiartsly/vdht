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

/*
 * forward the request to make host online
 */
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

/*
 *  forward the request to make host offline.
 */
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

/*
 * forward the request to make vdhtd exit.
 */
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

/*
 * forward the reqeust to dump all infos about host, so as to debug
 */
static
int _vlsctl_dump_host(struct vlsctl* lsctl, void* data, int offset)
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
int _vlsctl_bogus_query(struct vlsctl* lsctl, void* data, int offset)
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

    ret = _aux_get_addr(data, offset + sz, &sin);
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
int _vlsctl_join_node(struct vlsctl* lsctl, void* data, int offset)
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
    return sz;
}

/*
 * forward the reqeust to remove wellknown node or node with given address
 * from dht routing table.
 */
static
int _vlsctl_drop_node(struct vlsctl* lsctl, void* data, int offset)
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
    return sz;
}

/*
 *  forward to announcemaent about service information after service has been
 *  established and therefor can be provided for other nodes. The service can
 *  relay, stun, ddns, ...
 */
static
int _vlsctl_service_pub(struct vlsctl* lsctl, void* data, int offset)
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

/*
 * forward to the declaration of service for being unavaiable.
 */
static
int _vlsctl_service_unavai(struct vlsctl* lsctl, void* data, int offset)
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

/*
 * forward the request to get the best service option for special kind.
 */
static
int _vlsctl_service_prefer(struct vlsctl* lsctl, void* data, int offset)
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
int _vlsctl_dump_cfg(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    host->cfg->ops->dump(host->cfg);
    return 0;
}

static
int _vlsctl_dispatch(struct vlsctl* lsctl, void* data, int offset)
{
    int (*routine_cb[])(struct vlsctl*, void*, int) = {
        lsctl->ops->host_up,
        lsctl->ops->host_down,
        lsctl->ops->host_exit,
        lsctl->ops->dump_host,
        lsctl->ops->bogus_query,
        lsctl->ops->join_node,
        lsctl->ops->drop_node,
        lsctl->ops->srvc_pub,
        lsctl->ops->srvc_unavai,
        lsctl->ops->srvc_prefer,
        lsctl->ops->dump_host,
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
    .host_up      = _vlsctl_host_up,
    .host_down    = _vlsctl_host_down,
    .host_exit    = _vlsctl_host_exit,
    .dump_host    = _vlsctl_dump_host,
    .bogus_query  = _vlsctl_bogus_query,
    .join_node    = _vlsctl_join_node,
    .drop_node    = _vlsctl_drop_node,
    .srvc_pub     = _vlsctl_service_pub,
    .srvc_unavai  = _vlsctl_service_unavai,
    .srvc_prefer  = _vlsctl_service_prefer,
    .dump_cfg     = _vlsctl_dump_cfg,
    .dispatch     = _vlsctl_dispatch
};

int _aux_lsctl_usr_msg_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* lsctl = (struct vlsctl*)cookie;
    int ret = 0;

    ret = lsctl->ops->dispatch(lsctl, um->data, 0);
    retE((ret < 0));
    return 0;
}

static
int _aux_lsctl_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
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
    int ret = 0;

    vassert(ctl);
    vassert(host);

    sun->sun_family = AF_UNIX;
    ret = ops->get_str_ext(host->cfg, "lsctl.unix_path", sun->sun_path, 105, DEF_LSCTL_UNIX_PATH);
    retE((ret < 0));

    ctl->host = host;
    ctl->ops  = &lsctl_ops;

    ret += vmsger_init(&ctl->msger);
    ret += vrpc_init(&ctl->rpc, &ctl->msger, VRPC_UNIX, &ctl->addr);
    if (ret < 0) {
        vrpc_deinit(&ctl->rpc);
        vmsger_deinit(&ctl->msger);
        return -1;
    }

    vmsger_reg_unpack_cb  (&ctl->msger,   _aux_lsctl_unpack_msg_cb, ctl);
    ctl->msger.ops->add_cb(&ctl->msger, ctl, _aux_lsctl_usr_msg_cb, VMSG_LSCTL);
    return 0;
}

void vlsctl_deinit(struct vlsctl* ctl)
{
    vassert(ctl);

    vmsger_deinit(&ctl->msger);
    vrpc_deinit(&ctl->rpc);
    return ;
}

