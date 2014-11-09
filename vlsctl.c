#include "vglobal.h"
#include "vlsctl.h"

/* message interface for local service controller.*/
#define IS_LSCTL_MAGIC(val) (val == (uint32_t)0x7fec45fa)

/* try to start host.
 *----------------------------
 * lsctl_id -->|
 *----------------------------
 *
 * try to stop host.
 *----------------------------
 * VLSCTL_DHT_DOWN
 *----------------------------
 *
 * try to make host app exit.
 *----------------------------
 * VLSCTL_DHT_SHUTDOWN
 *----------------------------
 *
 * try to add a node with well-known address.
 *------------------------------
 * lsctl-id -->|<-IP->|<-port->|
 *------------------------------
 *
 * try to drop a node with well-known address.
 *------------------------------
 * lsctl-id -->|<-IP->|<-port->|
 *------------------------------
 *
 * try to register replay property.
 *----------------------------
 * VLSCTL_RELAY|
 *----------------------------
 */

static
int _aux_rt_dht_up(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    return host->ops->start(host);
}

static
int _aux_rt_dht_down(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    return host->ops->stop(host);
}

static
int _aux_rt_dht_exit(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    return host->ops->req_quit(host);
}

static
int _aux_rt_add_node(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    varg_decl(argv, 1, void*, addr);
    int sz = *(int*)argv[2];
    struct sockaddr_in sin;
    void* data = NULL;
    char ip[64];
    int  port = 0;
    int  ret  = 0;

    data = offset_addr(addr, sz);
    port = get_int32(data);
    sz  += sizeof(long);

    data = offset_addr(addr, sz);
    memset(ip, 0, 64);
    strcpy(ip, data);
    sz  += strlen(ip) + 1;

    ret = vsockaddr_convert(ip, port, &sin);
    retE((ret < 0));
    ret = host->ops->join(host, &sin);
    retE((ret < 0));

    vlogI(printf("[vlsctl] host joined a node (%s:%d)", ip, port));
    return sz;
}

static
int _aux_rt_del_node(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    varg_decl(argv, 1, void*, addr);
    int sz = *(int*)argv[2];
    struct sockaddr_in sin;
    void* data = NULL;
    char ip[64];
    int  port = 0;
    int  ret  = 0;

    data = offset_addr(addr, sz);
    port = get_int32(data);
    sz  += sizeof(long);

    data = offset_addr(addr, sz);
    memset(ip, 0, 64);
    strcpy(ip, data);
    sz  += strlen(ip) + 1;

    ret = vsockaddr_convert(ip, port, &sin);
    retE((ret < 0));
    ret = host->ops->drop(host, &sin);
    retE((ret < 0));

    vlogI(printf("[vlsctl] host dropped a node (%s:%d)", ip, port));
    return sz;
}

static
int _aux_rt_relay_up(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    //todo;
    vlogI(printf("[vlsctl] relay up"));
    return 0;
}

static
int _aux_rt_relay_down(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    //todo;
    vlogI(printf("[vlsctl] relay down"));
    return 0;
}

static
int _aux_rt_stun_up(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    //todo;
    vlogI(printf("[vlsctl] stun up"));
    return 0;
}

static
int _aux_rt_stun_down(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    //todo;
    vlogI(printf("[vlsctl] stun down"));
    return 0;
}


static
int _aux_rt_vpn_up(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    //todo;
    vlogI(printf("[vlsctl] vpn up"));
    return 0;
}

static
int _aux_rt_vpn_down(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    //todo;
    vlogI(printf("[vlsctl] vpn down"));
    return 0;
}

static
int _aux_rt_log_stdout(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    vlogI(printf("[vlsctl] request to dump host"));
    host->ops->dump(host);
    return 0;
}

static
int _aux_rt_cfg_stdout(void** argv)
{
    varg_decl(argv, 0, struct vhost*, host);
    vassert(host);

    vlogI(printf("[vlsctl] request to dump config"));
    host->cfg->ops->dump(host->cfg);
    return 0;
}

static
int (*lsctl_rt_t[])(void**) = {
    _aux_rt_dht_up,
    _aux_rt_dht_down,
    _aux_rt_dht_exit,
    _aux_rt_add_node,
    _aux_rt_del_node,
    _aux_rt_relay_up,
    _aux_rt_relay_down,
    _aux_rt_stun_up,
    _aux_rt_stun_down,
    _aux_rt_vpn_up,
    _aux_rt_vpn_down,
    _aux_rt_log_stdout,
    _aux_rt_cfg_stdout,
    NULL
};

static
int _aux_msg_parse_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* ctl = (struct vlsctl*)cookie;
    struct vhost* host = ctl->host;
    void* data = um->data;
    int lsctl_id = 0;
    int nitems = 0;
    int ret = 0;
    int sz  = 0;

    vassert(ctl);
    vassert(um);

    nitems = get_int32(um->data);
    sz += sizeof(long);
    vlogI(printf("[lsctl] received lsctl request (%d)", nitems));

    while(nitems-- > 0) {
        void* argv[3];

        data = offset_addr(um->data, sz);
        lsctl_id = get_uint32(data);
        sz += sizeof(uint32_t);

        argv[0] = host;
        argv[1] = um->data;
        argv[2] = &sz;

        retE((lsctl_id < 0));
        retE((lsctl_id >= VLSCTL_BUTT));

        ret = lsctl_rt_t[lsctl_id](argv);
        retE((ret < 0));
        sz += ret;
    }

    return 0;
}

static
int _aux_msg_unpack_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
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
    data = offset_addr(data, sizeof(long));
    sz += sizeof(long);

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
    ret = ops->get_str(host->cfg, "lsctl.unix_path", unix_path, BUF_SZ);
    if (ret < 0) {
        strcpy(unix_path, DEF_LSCTL_UNIX_PATH);
    }
    retE((strlen(unix_path) + 1 >= sizeof(sun->sun_path)));

    sun->sun_family = AF_UNIX;
    strcpy(sun->sun_path, unix_path);
    ctl->host = host;

    ret += vmsger_init(&ctl->msger);
    ret += vrpc_init(&ctl->rpc, &ctl->msger, VRPC_UNIX, &ctl->addr);
    if (ret < 0) {
        vrpc_deinit(&ctl->rpc);
        vmsger_deinit(&ctl->msger);
        return -1;
    }

    vmsger_reg_unpack_cb  (&ctl->msger, _aux_msg_unpack_cb, ctl);
    ctl->msger.ops->add_cb(&ctl->msger, ctl, _aux_msg_parse_cb, VMSG_LSCTL);
    return 0;
}

void vlsctl_deinit(struct vlsctl* ctl)
{
    vassert(ctl);

    vmsger_deinit(&ctl->msger);
    vrpc_deinit(&ctl->rpc);
    return ;
}

