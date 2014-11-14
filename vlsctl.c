#include "vglobal.h"
#include "vlsctl.h"

/* message interface for local service controller.*/
#define IS_LSCTL_MAGIC(val) (val == (uint32_t)0x7fec45fa)

static char* plugin_desc[] = {
    "relay",
    "stun",
    "vpn",
    "ddns",
    "multi_route",
    "data_hash",
    "app",
    NULL
};

static char* dht_query_desc[] = {
    "ping",
    "ping_r",
    "find_node",
    "find_node_r",
    "get_peers",
    "get_peers_r",
    "post_hash",
    "post_hash_r",
    "find_closest_nodes",
    "find_closest_nodes_r",
    NULL
};

static
int _vlsctl_dht_up(struct vlsctl* lsctl, void* data, int offset)
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
int _vlsctl_dht_down(struct vlsctl* lsctl, void* data, int offset)
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
int _vlsctl_dht_exit(struct vlsctl* lsctl, void* data, int offset)
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
int _vlsctl_dht_query(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    char ip[64];
    int port = 0;
    int qId  = 0;
    int ret  = 0;
    int sz   = 0;

    qId = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);
    vlogI(printf("[vlsctl] reqeust to send @%s query", dht_query_desc[qId]));

    port = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);

    memset(ip, 0, 64);
    strcpy(ip, (char*)offset_addr(data, offset + sz));
    sz += strlen(ip) + 1;

    ret = vsockaddr_convert(ip, port, &sin);
    retE((ret < 0));
    ret = host->ops->bogus_query(host, qId, &sin);
    retE((ret < 0));
    return 0;
}

static
int _vlsctl_add_node(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    char ip[64];
    int port = 0;
    int ret  = 0;
    int sz = 0;

    port = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);

    memset(ip, 0, 64);
    strcpy(ip, (char*)offset_addr(data, offset + sz));
    sz += strlen(ip) + 1;

    ret = vsockaddr_convert(ip, port, &sin);
    retE((ret < 0));
    ret = host->ops->join(host, &sin);
    retE((ret < 0));

    vlogI(printf("[vlsctl] host joined a node (%s:%d)", ip, port));
    return sz;
}

static
int _vlsctl_del_node(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    char ip[64];
    int port = 0;
    int ret  = 0;
    int sz   = 0;

    port = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);

    memset(ip, 0, 64);
    strcpy(ip, (char*)offset_addr(data, offset + sz));
    sz += strlen(ip) + 1;

    ret = vsockaddr_convert(ip, port, &sin);
    retE((ret < 0));
    ret = host->ops->drop(host, &sin);
    retE((ret < 0));

    vlogI(printf("[vlsctl] host dropped a node (%s:%d)", ip, port));
    return sz;
}

static
int _vlsctl_plug(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    int plgnId = 0;
    int ret = 0;
    int sz  = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    plgnId = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);
    retE((plgnId < PLUGIN_RELAY));
    retE((plgnId >= PLUGIN_BUTT));

    ret = host->ops->plug(host, plgnId);
    retE((ret < 0));
    vlogI(printf("plugin(%s) up.", plugin_desc[plgnId]));
    return 0;
}

static
int _vlsctl_unplug(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    int plgnId = 0;
    int ret = 0;
    int sz  = 0;

    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    plgnId = get_int32(offset_addr(data, offset + sz));
    sz += sizeof(int32_t);
    retE((plgnId < PLUGIN_RELAY));
    retE((plgnId >= PLUGIN_BUTT));

    ret = host->ops->unplug(host, plgnId);
    retE((ret < 0));
    vlogI(printf("plugin(%s) down.", plugin_desc[plgnId]));
    return 0;
}

static
int _vlsctl_log_stdout(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    vlogI(printf("[vlsctl] request to dump host"));
    host->ops->dump(host);
    return 0;
}

static
int _vlsctl_cfg_stdout(struct vlsctl* lsctl, void* data, int offset)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(data);
    vassert(offset > 0);

    vlogI(printf("[vlsctl] request to dump config"));
    host->cfg->ops->dump(host->cfg);
    return 0;
}

static
struct vlsctl_ops lsctl_ops = {
    .dht_up     = _vlsctl_dht_up,
    .dht_down   = _vlsctl_dht_down,
    .dht_exit   = _vlsctl_dht_exit,
    .dht_query  = _vlsctl_dht_query,
    .add_node   = _vlsctl_add_node,
    .del_node   = _vlsctl_del_node,
    .plug       = _vlsctl_plug,
    .unplug     = _vlsctl_unplug,
    .log_stdout = _vlsctl_log_stdout,
    .cfg_stdout = _vlsctl_cfg_stdout
};

static
int _aux_msg_parse_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* ctl = (struct vlsctl*)cookie;
    void* data = um->data;
    int lsctl_id = 0;
    int nitems = 0;
    int ret = 0;
    int sz  = 0;

    vassert(ctl);
    vassert(um);

    nitems = get_int32(um->data);
    sz += sizeof(int32_t);
    vlogI(printf("[lsctl] received lsctl request (%d)", nitems));

    while(nitems-- > 0) {
        data = offset_addr(um->data, sz);
        lsctl_id = get_uint32(data);
        sz += sizeof(uint32_t);

        retE((lsctl_id < 0));
        retE((lsctl_id >= VLSCTL_BUTT));

        switch(lsctl_id) {
        case VLSCTL_DHT_UP:
            ret = ctl->ops->dht_up(ctl, um->data, sz);
            break;
        case VLSCTL_DHT_DOWN:
            ret = ctl->ops->dht_down(ctl, um->data, sz);
            break;
        case VLSCTL_DHT_EXIT:
            ret = ctl->ops->dht_exit(ctl, um->data, sz);
            break;
        case VLSCTL_DHT_QUERY:
            ret = ctl->ops->dht_query(ctl, um->data, sz);
            break;
        case VLSCTL_ADD_NODE:
            ret = ctl->ops->add_node(ctl, um->data, sz);
            break;
        case VLSCTL_DEL_NODE:
            ret = ctl->ops->del_node(ctl, um->data, sz);
            break;
        case VLSCTL_PLUG:
            ret = ctl->ops->plug(ctl, um->data, sz);
            break;
        case VLSCTL_UNPLUG:
            ret = ctl->ops->unplug(ctl, um->data, sz);
            break;
        case VLSCTL_LOGOUT:
            ret = ctl->ops->log_stdout(ctl, um->data, sz);
            break;
        case VLSCTL_CFGOUT:
            ret = ctl->ops->cfg_stdout(ctl, um->data, sz);
            break;
        default:
            vassert(0);
        }
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
    ret = ops->get_str(host->cfg, "lsctl.unix_path", unix_path, BUF_SZ);
    if (ret < 0) {
        strcpy(unix_path, DEF_LSCTL_UNIX_PATH);
    }
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

