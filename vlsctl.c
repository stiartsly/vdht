#include "vglobal.h"
#include "vlsctl.h"

static
int _aux_lsctl_unpack_addr(void* buf, int len, struct sockaddr_in* addr)
{
    uint16_t port = 0;
    uint32_t saddr = 0;
    int tsz = 0;
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(addr);

    tsz += sizeof(uint16_t); //skip faimly value;
    port   = *(uint16_t*)(buf + tsz);
    tsz += sizeof(uint16_t);
    saddr  = *(uint32_t*)(buf + tsz);

    ret = vsockaddr_convert2(saddr, port, addr);
    retE((ret < 0));
    return tsz;
}

/*
 * forward the request to make host online
 */
static
int _vlsctl_unpack_cmd_host_up(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    int ret = 0;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    ret = host->ops->start(host);
    retE((ret < 0));
    return 0;
}

/*
 *  forward the request to make host offline.
 */
static
int _vlsctl_unpack_cmd_host_down(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    int ret = 0;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    ret = host->ops->stop(host);
    retE((ret < 0));
    return 0;
}

/*
 * forward the request to make vdhtd exit.
 */
static
int _vlsctl_unpack_cmd_host_exit(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    ret = host->ops->exit(host);
    retE((ret < 0));
    return 0;
}

/*
 * forward the reqeust to dump all infos about host, so as to debug
 */
static
int _vlsctl_unpack_cmd_host_dump(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    host->ops->dump(host);
    return 0;
}

/*
 * forward the reqeust to dump config
 */
static
int _vlsctl_unpack_cmd_cfg_dump(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    host->cfg->ops->dump(host->cfg);
    return 0;
}

/*
 * forward the request to add wellknown node or node with given address
 * into dht routing table.
 */
static
int _vlsctl_unpack_cmd_join_node(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    ret = _aux_lsctl_unpack_addr(buf, len, &sin);
    retE((ret < 0));
    tsz += ret;

    ret = host->ops->join(host, &sin);
    retE((ret < 0));
    return tsz;
}

/*
 * forward the request to send query to the node with given address.
 * this request is purely for debug.
 */
static
int _vlsctl_unpack_cmd_bogus_query(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in sin;
    int qId = 0;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    qId = *(int32_t*)(buf + tsz);
    tsz += sizeof(int32_t);

    ret = _aux_lsctl_unpack_addr(buf + tsz, len - tsz, &sin);
    retE((ret < 0));
    tsz += ret;

    ret = host->ops->bogus_query(host, qId, &sin);
    retE((ret < 0));
    vlogI("[vlsctl] send a query (@%s)", vdht_get_desc(qId));
    return tsz;
}

/*
 *  command to post service information after service is ready. the service
 *  can be stun, relay, ddns, or user customized service.
 */
static
int _vlsctl_unpack_cmd_post_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in addr;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    ret = _aux_lsctl_unpack_addr(buf + tsz, len - tsz, &addr);
    retE((ret < 0));
    tsz += ret;

    ret = host->srvc_ops->post(host, &hash, &addr);
    retE((ret < 0));
    return tsz;
}

/*
 * command to unpost (or reclaim) service.
 */
static
int _vlsctl_unpack_cmd_unpost_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    struct sockaddr_in addr;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    ret = _aux_lsctl_unpack_addr(buf + tsz, len - tsz, &addr);
    retE((ret < 0));
    tsz += ret;

    ret = host->srvc_ops->unpost(host, &hash, &addr);
    retE((ret < 0));
    return tsz;
}

static
void _aux_vlsctl_iterate_addr_cb(struct sockaddr_in* addr, void* cookie)
{
    vassert(addr);

    printf(" ");
    vsockaddr_dump(addr);
    return ;
}

/*
 * command to probe best service for special term;
 */
static
int _vlsctl_unpack_cmd_probe_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vhost* host = lsctl->host;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    ret = host->srvc_ops->probe(host, &hash, _aux_vlsctl_iterate_addr_cb, NULL);
    retE((ret < 0));
    return tsz;
}

static
struct vlsctl_unpack_cmd_desc lsctl_cmds[] = {
    {"host up",       VLSCTL_HOST_UP,        _vlsctl_unpack_cmd_host_up       },
    {"host down",     VLSCTL_HOST_DOWN,      _vlsctl_unpack_cmd_host_down     },
    {"host exit",     VLSCTL_HOST_EXIT,      _vlsctl_unpack_cmd_host_exit     },
    {"host dump",     VLSCTL_HOST_DUMP,      _vlsctl_unpack_cmd_host_dump     },
    {"cfg dump",      VLSCTL_CFG_DUMP,       _vlsctl_unpack_cmd_cfg_dump      },
    {"join node",     VLSCTL_JOIN_NODE,      _vlsctl_unpack_cmd_join_node     },
    {"bogus query",   VLSCTL_BOGUS_QUERY,    _vlsctl_unpack_cmd_bogus_query   },
    {"post service",  VLSCTL_POST_SERVICE,   _vlsctl_unpack_cmd_post_service  },
    {"unpost service",VLSCTL_UNPOST_SERVICE, _vlsctl_unpack_cmd_unpost_service},
    {"probe service", VLSCTL_PROBE_SERVICE,  _vlsctl_unpack_cmd_probe_service },
    {NULL, VLSCTL_BUTT, NULL}
};

static
int _vlsctl_unpack_cmds(struct vlsctl* lsctl, void* buf, int len)
{
    struct vlsctl_unpack_cmd_desc* desc = lsctl_cmds;
    uint16_t blen  = 0;
    uint32_t magic = 0;
    int tsz = 0;

    tsz += sizeof(uint8_t); // skip version.
    tsz += sizeof(uint8_t); // skip type; todo;
    blen = *(uint16_t*)(buf + tsz);
    tsz += sizeof(uint16_t);
    magic = *(uint32_t*)(buf + tsz);
    tsz += sizeof(uint32_t);

    if (magic != VLSCTL_MAGIC) {
        vlogE("wrong lsctl magic(0x%x), should be(0x%x)", magic, VLSCTL_MAGIC);
        retE((1));
    }
    while (blen > 0) {
        uint16_t cid  = 0;
        uint16_t clen = 0;
        int ret = 0;
        int i = 0;

        cid   = *(uint16_t*)(buf + tsz);
        tsz  += sizeof(uint16_t);
        blen -= sizeof(uint16_t);
        clen = *(uint16_t*)(buf + tsz);
        tsz  += sizeof(uint16_t);
        blen -= sizeof(uint16_t);

        for(i = 0; desc[i].cmd; i++) {
            if (desc[i].cmd_id != (uint32_t)cid) {
                continue;
            }
            ret = desc[i].cmd(lsctl, buf + tsz, clen);
            retE((ret < 0));
            tsz  += ret;
            blen -= ret;
            break;
        }
    }
    vassert(blen == 0);
    return 0;
}

static
struct vlsctl_ops lsctl_ops = {
    .unpack_cmds = _vlsctl_unpack_cmds
};

int _aux_lsctl_usr_msg_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* lsctl = (struct vlsctl*)cookie;
    int ret = 0;

    vassert(lsctl);
    vassert(um->data);

    ret = lsctl->ops->unpack_cmds(lsctl, um->data, um->len);
    retE((ret < 0));
    return 0;
}

static
int _aux_lsctl_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    vassert(sm);
    vassert(um);

    vmsg_usr_init(um, VMSG_LSCTL, &sm->addr, NULL, sm->len, sm->data);
    return 0;
}

int vlsctl_init(struct vlsctl* lsctl, struct vhost* host, struct vconfig* cfg)
{
    struct sockaddr_un* sun = to_sockaddr_sun(&lsctl->addr);
    int ret = 0;

    vassert(lsctl);
    vassert(host);
    vassert(cfg);

    sun->sun_family = AF_UNIX;
    strncpy(sun->sun_path, cfg->ext_ops->get_lsctl_socket(cfg), 105);

    lsctl->host = host;
    lsctl->ops  = &lsctl_ops;

    ret += vmsger_init(&lsctl->msger);
    ret += vrpc_init(&lsctl->rpc, &lsctl->msger, VRPC_UNIX, &lsctl->addr);
    if (ret < 0) {
        vrpc_deinit(&lsctl->rpc);
        vmsger_deinit(&lsctl->msger);
        retE((1));
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

