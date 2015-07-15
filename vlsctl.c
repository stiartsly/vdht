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

    tsz  += sizeof(uint16_t); //skip faimly value;
    port  = ntohs(get_uint16(buf + tsz));
    tsz  += sizeof(uint16_t);
    saddr = ntohl(get_uint32(buf + tsz));
    tsz  += sizeof(uint32_t);

    ret = vsockaddr_convert2(saddr, port, addr);
    retE((ret < 0));
    return tsz;
}

static
int _aux_lsctl_unpack_vaddr(void* buf, int len, struct vsockaddr_in* addr)
{
    uint16_t port  = 0;
    uint32_t saddr = 0;
    uint32_t type  = 0;
    int tsz = 0;
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(addr);

    tsz  += sizeof(uint16_t); //skip faimly value;
    port  = ntohs(get_uint16(buf + tsz));
    tsz  += sizeof(uint16_t);
    saddr = ntohl(get_uint32(buf + tsz));
    tsz  += sizeof(uint32_t);
    type  = get_uint32(buf + tsz);
    tsz  += sizeof(uint32_t);

    ret = vsockaddr_convert2(saddr, port, &addr->addr);
    retE((ret < 0));
    addr->type = type;
    return tsz;
}

/*
 * the routine to execute command @host_up from @vlsctlc
 */
static
int _vlsctl_exec_cmd_host_up(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    int ret = 0;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);
    (void)from;

    ret = app->api_ops->start_host(app);
    retE((ret < 0));
    return 0;
}

/*
 *  the routine to execute command @host_down.
 */
static
int _vlsctl_exec_cmd_host_down(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    int ret = 0;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);
    (void)from;

    ret = app->api_ops->stop_host(app);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to execute command @host_exit.
 */
static
int _vlsctl_exec_cmd_host_exit(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);
    (void)from;

    app->api_ops->make_host_exit(app);
    return 0;
}

/*
 * the routine to execute command @host_dump.
 */
static
int _vlsctl_exec_cmd_host_dump(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);
    (void)from;

    app->api_ops->dump_host_info(app);
    return 0;
}

/*
 * forward the reqeust to dump config
 */
static
int _vlsctl_exec_cmd_cfg_dump(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    vassert(lsctl);
    vassert(buf);
    vassert(len >= 0);
    (void)from;

    app->api_ops->dump_cfg_info(app);
    return 0;
}

/*
 * the routine to execute command @join_node.
 */
static
int _vlsctl_exec_cmd_join_node(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    (void)from;

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
int _vlsctl_exec_cmd_bogus_query(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    int qId = 0;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    (void)from;

    qId = get_int32(buf + tsz);
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
int _vlsctl_exec_cmd_post_service(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    struct vsockaddr_in addr;
    vsrvcHash hash;
    int proto = 0;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    (void)from;

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    proto = get_uint32(buf + tsz);
    tsz += sizeof(int32_t);

    while(len - tsz > 0) {
        ret = _aux_lsctl_unpack_vaddr(buf + tsz, len - tsz, &addr);
        retE((ret < 0));
        tsz += ret;

        ret = app->api_ops->post_service_segment(app, &hash, &addr, proto);
        retE((ret < 0));
    }
    return tsz;
}

/*
 * the routine to execute command @unpost_service.
 */
static
int _vlsctl_exec_cmd_unpost_service(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vappmain* app = lsctl->app;
    struct sockaddr_in addr;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    (void)from;

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
        // if no address follows, it means to unpost whole service.
        ret = app->api_ops->unpost_service(app, &hash);
        retE((ret < 0));
    }
    return tsz;
}

static
void _aux_vlsctl_number_addr_cb1(vsrvcHash* hash, int naddrs, int proto, void* cookie)
{
    struct find_service_rsp_args* args = (struct find_service_rsp_args*)cookie;
    vassert(hash);
    vassert(naddrs >= 0);

    args->total = naddrs;
    args->proto = proto;
    return ;
}

static
void _aux_vlsctl_iterate_addr_cb1(vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int last, void* cookie)
{
    struct find_service_rsp_args* args = (struct find_service_rsp_args*)cookie;
    int index = args->index;

    vassert(hash);
    vassert(addr);

    vsockaddr_copy(&args->addrs[index].addr, addr);
    args->addrs[index].type = type;
    args->index++;
    return ;
}

/*
 * the routine to execute command @find service in service routing space.
 */
static
int _vlsctl_exec_cmd_find_service(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vlsctl_rsp_args*   rsp_args = NULL;
    struct find_service_rsp_args* args = NULL;
    struct vappmain* app = lsctl->app;
    char* mbuf = NULL;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    vassert(from);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    rsp_args = (struct vlsctl_rsp_args*)malloc(sizeof(*rsp_args));
    vlogEv((!rsp_args), elog_malloc);
    retE((!rsp_args));
    memset(rsp_args, 0, sizeof(*rsp_args));

    rsp_args->cmd_id = VLSCTL_FIND_SERVICE;
    args = &rsp_args->uargs.find_service_rsp_args;
    args->total = 0;
    args->index = 0;
    args->pack_cb = lsctl->pack_cmd_ops->find_service_rsp;
    vtoken_copy(&args->hash, &hash);

    ret = app->api_ops->find_service(app, &hash, _aux_vlsctl_number_addr_cb1, _aux_vlsctl_iterate_addr_cb1, args);
    ret1E((ret < 0), free(rsp_args));

    mbuf  = (char*)vmsg_buf_alloc(0);
    ret1E((!mbuf), free(rsp_args));

    ret = lsctl->ops->pack_cmd(lsctl, mbuf, BUF_SZ, rsp_args);
    free(rsp_args);
    ret1E((ret < 0), vmsg_buf_free(mbuf));
    {
        struct vmsg_usr msg = {
            .addr  = from,
            .spec  = NULL,
            .msgId = VMSG_LSCTL,
            .data  = mbuf,
            .len   = ret
        };
        ret = lsctl->msger.ops->push(&lsctl->msger, &msg);
        ret1E((ret < 0), vmsg_buf_free(mbuf));
    }
    return tsz;
}

static
void _aux_vlsctl_number_addr_cb2(vsrvcHash* hash, int naddrs, int proto, void* cookie)
{
    struct vlsctl_rsp_args*    rsp_args = (struct vlsctl_rsp_args*)cookie;
    struct probe_service_rsp_args* args = &rsp_args->uargs.probe_service_rsp_args;
    struct vlsctl* lsctl = args->lsctl;
    char* mbuf = NULL;
    int ret = 0;

    vassert(hash);
    vassert(naddrs >= 0);

    args->total = naddrs;
    args->proto = proto;

    if (!naddrs) {
        mbuf = (char*)vmsg_buf_alloc(0);
        ret1E_v((!mbuf), free(rsp_args));

        ret = lsctl->ops->pack_cmd(lsctl, mbuf, BUF_SZ, rsp_args);
        free(rsp_args);
        ret1E_v((ret < 0), vmsg_buf_free(mbuf));
        {
            struct vmsg_usr msg = {
                .addr  = &args->from,
                .spec  = NULL,
                .msgId = VMSG_LSCTL,
                .data  = mbuf,
                .len   = ret
            };
            ret = lsctl->msger.ops->push(&lsctl->msger, &msg);
            ret1E_v((ret < 0), vmsg_buf_free(mbuf));
        }
    }
    return ;
}

static
void _aux_vlsctl_iterate_addr_cb2(vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int last, void* cookie)
{
    struct vlsctl_rsp_args*    rsp_args = (struct vlsctl_rsp_args*)cookie;
    struct probe_service_rsp_args* args = &rsp_args->uargs.probe_service_rsp_args;
    struct vlsctl* lsctl = args->lsctl;
    int index = args->index;
    char* mbuf = NULL;
    int ret = 0;

    vsockaddr_copy(&args->addrs[index].addr, addr);
    args->addrs[index].type = type;
    args->index++;

    if (last) {
        mbuf = (char*)vmsg_buf_alloc(0);
        ret1E_v((!mbuf), free(rsp_args));

        ret = lsctl->ops->pack_cmd(lsctl, mbuf, BUF_SZ, rsp_args);
        free(rsp_args);
        ret1E_v((ret < 0), vmsg_buf_free(mbuf));
        {
            struct vmsg_usr msg = {
                .addr  = &args->from,
                .spec  = NULL,
                .msgId = VMSG_LSCTL,
                .data  = mbuf,
                .len   = ret
            };
            ret = lsctl->msger.ops->push(&lsctl->msger, &msg);
            ret1E_v((ret < 0), vmsg_buf_free(mbuf));
        }
    }
    return ;
}

/*
 * the routine to execute command @probe service.
 */
static
int _vlsctl_exec_cmd_probe_service(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vlsctl_rsp_args*    rsp_args = NULL;
    struct probe_service_rsp_args* args = NULL;
    struct vappmain* app = lsctl->app;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    vassert(from);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    rsp_args = (struct vlsctl_rsp_args*)malloc(sizeof(*rsp_args));
    vlogEv((!rsp_args), elog_malloc);
    retE((!rsp_args));
    memset(rsp_args, 0, sizeof(*rsp_args));

    rsp_args->cmd_id = VLSCTL_PROBE_SERVICE;
    args = &rsp_args->uargs.probe_service_rsp_args;
    args->total = 0;
    args->index = 0;
    args->lsctl = lsctl;
    args->pack_cb = lsctl->pack_cmd_ops->probe_service_rsp;
    vtoken_copy(&args->hash, &hash);
    memcpy(&args->from, from, sizeof(*from));

    ret = app->api_ops->probe_service(app, &hash, _aux_vlsctl_number_addr_cb2, _aux_vlsctl_iterate_addr_cb2, rsp_args);
    ret1E((ret < 0), free(rsp_args));
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
int _aux_vlsctl_pack_vaddr(void* buf, int len, struct vsockaddr_in* addr)
{
    int tsz = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    set_uint8(buf + tsz, 0);
    tsz += sizeof(uint8_t);
    set_uint8(buf + tsz,  addr->addr.sin_family);
    tsz += sizeof(uint8_t);
    set_uint16(buf + tsz, addr->addr.sin_port);
    tsz += sizeof(uint16_t);
    set_uint32(buf + tsz, addr->addr.sin_addr.s_addr);
    tsz += sizeof(uint32_t);
    set_uint32(buf + tsz, addr->type);
    tsz += sizeof(uint32_t);

    return tsz;
}

static
int _vlsctl_pack_find_service_rsp(void* buf, int len, void* cookie)
{
    struct find_service_rsp_args* args = (struct find_service_rsp_args*)cookie;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;
    int i = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(args);

    set_uint16(buf + tsz, VLSCTL_FIND_SERVICE);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);
    set_int32(buf + tsz, args->proto);
    bsz += sizeof(int32_t);
    tsz += sizeof(int32_t);
    for (i = 0; i < args->total; i++) {
        ret = _aux_vlsctl_pack_vaddr(buf + tsz, len - tsz, &args->addrs[i]);
        tsz += ret;
        bsz += ret;
    }
    set_uint16(len_addr, bsz);
    return tsz;
}

static
int _vlsctl_pack_probe_service_rsp(void* buf, int len, void* cookie)
{
    struct probe_service_rsp_args* args = (struct probe_service_rsp_args*)cookie;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;
    int i = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(args);

    set_uint16(buf + tsz, VLSCTL_PROBE_SERVICE);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);
    set_int32(buf + tsz, args->proto);
    bsz += sizeof(int32_t);
    tsz += sizeof(int32_t);
    for (i = 0; i < args->total; i++) {
        ret = _aux_vlsctl_pack_vaddr(buf + tsz, len - tsz, &args->addrs[i]);
        tsz += ret;
        bsz += ret;
    }
    set_uint16(len_addr, bsz);
    return tsz;
}

static
struct vlsctl_pack_cmd_ops lsctl_pack_cmd_ops = {
    .find_service_rsp  = _vlsctl_pack_find_service_rsp,
    .probe_service_rsp = _vlsctl_pack_probe_service_rsp
};

static
int _vlsctl_unpack_cmd(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    struct vlsctl_exec_cmd_desc* desc = lsctl_cmds;
    uint16_t blen  = 0;
    uint32_t magic = 0;
    int tsz = 0;

    tsz += sizeof(uint8_t); // skip version.
    tsz += sizeof(uint8_t); // skip type; todo;
    blen = get_uint16(buf + tsz);
    tsz += sizeof(uint16_t);
    magic = get_uint32(buf + tsz);
    tsz += sizeof(uint32_t);

    if (magic != VLSCTL_MAGIC) {
        vlogE("wrong lsctl magic(0x%x), should be(0x%x)", magic, VLSCTL_MAGIC);
        retE((1));
    }
    while (blen > 0) {
        uint16_t cid  = 0;
        uint16_t clen = 0;
        int ret = 0;

        cid   = get_uint16(buf + tsz);
        tsz  += sizeof(uint16_t);
        blen -= sizeof(uint16_t);
        clen  = get_uint16(buf + tsz);
        tsz  += sizeof(uint16_t);
        blen -= sizeof(uint16_t);

        for (; desc->cmd; desc++) {
            if (desc->cmd_id != cid) {
                continue;
            }
            ret = desc->cmd(lsctl, buf + tsz, clen, from);
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
int _vlsctl_pack_cmd(struct vlsctl* lsctl, void* buf, int len, void* cookie)
{
    struct vlsctl_rsp_args* rsp_args = (struct vlsctl_rsp_args*)cookie;
    void* len_addr = NULL;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    set_uint8(buf + tsz, VLSCTL_VERSION);
    tsz += sizeof(uint8_t);
    set_uint8(buf + tsz, vlsctl_rsp_succ);
    tsz += sizeof(uint8_t);

    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);
    set_uint32(buf + tsz, VLSCTL_MAGIC);
    tsz += sizeof(uint32_t);

    switch(rsp_args->cmd_id) {
    case VLSCTL_FIND_SERVICE: {
        struct find_service_rsp_args* args = &rsp_args->uargs.find_service_rsp_args;
        ret = args->pack_cb(buf + tsz, len - tsz, args);
        break;
    }
    case VLSCTL_PROBE_SERVICE: {
        struct probe_service_rsp_args* args = &rsp_args->uargs.probe_service_rsp_args;
        ret = args->pack_cb(buf + tsz, len - tsz, args);
        break;
    }
    default:
        retE((1));
        break;
    }
    tsz += ret;
    set_uint16(len_addr, ret);

    vhexbuf_dump(buf, tsz);
    return tsz;
}

static
struct vlsctl_ops lsctl_ops = {
    .unpack_cmd = _vlsctl_unpack_cmd,
    .pack_cmd   = _vlsctl_pack_cmd
};

int _aux_lsctl_usr_msg_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* lsctl = (struct vlsctl*)cookie;
    int ret = 0;

    vassert(lsctl);
    vassert(um->data);

    ret = lsctl->ops->unpack_cmd(lsctl, um->data, um->len, um->addr);
    retE((ret < 0));
    return 0;
}

static
int _aux_lsctl_pack_msg_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys* sm)
{
    vassert(um);
    vassert(sm);

    vmsg_sys_init(sm, um->addr, NULL, um->len, um->data);
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
    lsctl->pack_cmd_ops = &lsctl_pack_cmd_ops;

    ret += vmsger_init(&lsctl->msger);
    ret += vrpc_init(&lsctl->rpc, &lsctl->msger, VRPC_UNIX, &lsctl->addr);
    if (ret < 0) {
        vrpc_deinit(&lsctl->rpc);
        vmsger_deinit(&lsctl->msger);
        retE((1));
    }

    vmsger_reg_pack_cb  (&lsctl->msger, _aux_lsctl_pack_msg_cb,   lsctl);
    vmsger_reg_unpack_cb(&lsctl->msger, _aux_lsctl_unpack_msg_cb, lsctl);
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

