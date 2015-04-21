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
    port  = *(uint16_t*)(buf + tsz);
    port  = ntohs(port);
    tsz += sizeof(uint16_t);
    saddr = *(uint32_t*)(buf + tsz);
    saddr = ntohl(saddr);

    ret = vsockaddr_convert2(saddr, port, addr);
    retE((ret < 0));
    return tsz;
}

/*
 * the routine to execute command @host_up from @vlsctlc
 */
static
int _vlsctl_exec_cmd_host_up(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    int ret = 0;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    ret = app->api_ops->start_host(app);
    retE((ret < 0));
    return 0;
}

/*
 *  the routine to execute command @host_down.
 */
static
int _vlsctl_exec_cmd_host_down(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    int ret = 0;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    ret = app->api_ops->stop_host(app);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to execute command @host_exit.
 */
static
int _vlsctl_exec_cmd_host_exit(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    app->api_ops->make_host_exit(app);
    return 0;
}

/*
 * the routine to execute command @host_dump.
 */
static
int _vlsctl_exec_cmd_host_dump(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    app->api_ops->dump_host_info(app);
    return 0;
}

/*
 * forward the reqeust to dump config
 */
static
int _vlsctl_exec_cmd_cfg_dump(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);

    app->api_ops->dump_cfg_info(app);
    return 0;
}

/*
 * the routine to execute command @join_node.
 */
static
int _vlsctl_exec_cmd_join_node(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    ret = _aux_lsctl_unpack_addr(buf, len, &addr);
    retE((ret < 0));
    tsz += ret;
    vsockaddr_dump(&addr);

    ret = app->api_ops->join_wellknown_node(app, &addr);
    retE((ret < 0));
    return tsz;
}

/*
 * the routine to execute command @bogus_query, which only supports @ping query.
 */
static
int _vlsctl_exec_cmd_bogus_query(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    int qId = 0;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    qId = *(int32_t*)(buf + tsz);
    tsz += sizeof(int32_t);

    ret = _aux_lsctl_unpack_addr(buf + tsz, len - tsz, &addr);
    retE((ret < 0));
    tsz += ret;

    ret = app->api_ops->bogus_query(app, qId, &addr);
    retE((ret < 0));
    vlogI("[vlsctl] send a query (@%s)", vdht_get_desc(qId));
    return tsz;
}

/*
 *  the routine to exeucte command @post_service.
 */
static
int _vlsctl_exec_cmd_post_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    while(len - tsz > 0) {
        ret = _aux_lsctl_unpack_addr(buf + tsz, len - tsz, &addr);
        retE((ret < 0));
        tsz += ret;

        ret = app->api_ops->post_service_segment(app, &hash, &addr);
        retE((ret < 0));
    }
    return tsz;
}

/*
 * the routine to execute command @unpost_service.
 */
static
int _vlsctl_exec_cmd_unpost_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    while(len - tsz > 0) {
        ret = _aux_lsctl_unpack_addr(buf + tsz, len - tsz, &addr);
        retE((ret < 0));
        tsz += ret;

        ret = app->api_ops->unpost_service_segment(app, &hash, &addr);
        retE((ret < 0));
    }
    if (tsz <= VTOKEN_LEN) {
        // no addresses followed, means to unpost total service.
        ret = app->api_ops->unpost_service(app, &hash);
        retE((ret < 0));
    }
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
 * the routine to execute command @find service in service routing space.
 */
static
int _vlsctl_exec_cmd_find_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    ret = app->api_ops->find_service(app, &hash, _aux_vlsctl_iterate_addr_cb, NULL);
    retE((ret < 0));
    return tsz;
}

/*
 * the routine to execute command @probe service.
 */
static
int _vlsctl_exec_cmd_probe_service(struct vlsctl* lsctl, void* buf, int len)
{
    struct vappmain* app = lsctl->app;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    ret = app->api_ops->probe_service(app, &hash, _aux_vlsctl_iterate_addr_cb, NULL);
    retE((ret < 0));
    return tsz;
}

static
struct vlsctl_exec_cmd_desc lsctl_cmds[] = {
    {"host up",       VLSCTL_HOST_UP,        _vlsctl_exec_cmd_host_up       },
    {"host down",     VLSCTL_HOST_DOWN,      _vlsctl_exec_cmd_host_down     },
    {"host exit",     VLSCTL_HOST_EXIT,      _vlsctl_exec_cmd_host_exit     },
    {"host dump",     VLSCTL_HOST_DUMP,      _vlsctl_exec_cmd_host_dump     },
    {"cfg dump",      VLSCTL_CFG_DUMP,       _vlsctl_exec_cmd_cfg_dump      },
    {"join node",     VLSCTL_JOIN_NODE,      _vlsctl_exec_cmd_join_node     },
    {"bogus query",   VLSCTL_BOGUS_QUERY,    _vlsctl_exec_cmd_bogus_query   },
    {"post service",  VLSCTL_POST_SERVICE,   _vlsctl_exec_cmd_post_service  },
    {"unpost service",VLSCTL_UNPOST_SERVICE, _vlsctl_exec_cmd_unpost_service},
    {"find service",  VLSCTL_FIND_SERVICE,   _vlsctl_exec_cmd_find_service  },
    {"probe service", VLSCTL_PROBE_SERVICE,  _vlsctl_exec_cmd_probe_service },
    {NULL, VLSCTL_BUTT, NULL}
};

static
int _vlsctl_unpack_cmds(struct vlsctl* lsctl, void* buf, int len)
{
    struct vlsctl_exec_cmd_desc* desc = lsctl_cmds;
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

int vlsctl_init(struct vlsctl* lsctl, struct vappmain* app, struct vconfig* cfg)
{
    struct sockaddr_un* sun = to_sockaddr_sun(&lsctl->addr);
    int ret = 0;

    vassert(lsctl);
    vassert(app);
    vassert(cfg);

    sun->sun_family = AF_UNIX;
    strncpy(sun->sun_path, cfg->ext_ops->get_lsctl_socket(cfg), 105);

    lsctl->app  = app;
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

