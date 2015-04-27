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
int _vlsctl_exec_cmd_post_service(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
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

        ret = app->api_ops->post_service_segment(app, &hash, &addr);
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
        // no addresses followed, means to unpost total service.
        ret = app->api_ops->unpost_service(app, &hash);
        retE((ret < 0));
    }
    return tsz;
}

static
void _aux_vlsctl_number_addr_cb1(vsrvcHash* hash, int naddrs, void* cookie)
{
    union  vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    vassert(hash);
    vassert(naddrs >= 0);

    args->find_service_rsp_args.total = naddrs;
    return ;
}

static
void _aux_vlsctl_iterate_addr_cb1(vsrvcHash* hash, struct sockaddr_in* addr, int last, void* cookie)
{
    union vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    vassert(hash);
    vassert(addr);

    vsockaddr_copy(&args->find_service_rsp_args.addrs[args->find_service_rsp_args.index], addr);
    args->find_service_rsp_args.index++;
    return ;
}

/*
 * the routine to execute command @find service in service routing space.
 */
static
int _vlsctl_exec_cmd_find_service(struct vlsctl* lsctl, void* buf, int len, struct vsockaddr* from)
{
    union vlsctl_rsp_args * args = NULL;
    struct vappmain* app = lsctl->app;
    char* snd_buf = NULL;
    vsrvcHash hash;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);
    vassert(from);

    memcpy(hash.data, buf + tsz, VTOKEN_LEN);
    tsz += VTOKEN_LEN;

    args = (union vlsctl_rsp_args*)malloc(sizeof(*args));
    vlogEv((!args), elog_malloc);
    retE((!args));
    memset(args, 0, sizeof(*args));

    args->find_service_rsp_args.total = 0;
    args->find_service_rsp_args.index = 0;
    args->find_service_rsp_args.pack_cb = lsctl->pack_cmd_ops->find_service_rsp;
    vtoken_copy(&args->find_service_rsp_args.hash, &hash);
    ret = app->api_ops->find_service(app, &hash, _aux_vlsctl_number_addr_cb1, _aux_vlsctl_iterate_addr_cb1, args);
    ret1E((ret < 0), free(args));

    snd_buf = (char*)malloc(BUF_SZ);
    vlogEv((!snd_buf), elog_malloc);
    ret1E((!snd_buf), free(args));
    memset(snd_buf, 0, BUF_SZ);

    ret = lsctl->ops->pack_cmd(lsctl, snd_buf, BUF_SZ, args);
    free(args);
    ret1E((ret < 0), free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = from,
            .spec  = NULL,
            .msgId = VMSG_LSCTL,
            .data  = snd_buf,
            .len   = ret
        };
        ret = lsctl->msger.ops->push(&lsctl->msger, &msg);
        ret1E((ret < 0), free(snd_buf));
    }
    return tsz;
}

static
void _aux_vlsctl_number_addr_cb2(vsrvcHash* hash, int naddrs, void* cookie)
{
    union  vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    struct vlsctl* lsctl = args->probe_service_rsp_args.lsctl;
    char* buf = NULL;
    int ret = 0;

    vassert(hash);
    vassert(naddrs >= 0);

    args->probe_service_rsp_args.total = naddrs;
    if (!naddrs) {
        buf = (char*)malloc(BUF_SZ);
        vlogEv((!buf), elog_malloc);
        ret1E_v((!buf), free(args));
        memset(buf, 0, BUF_SZ);

        ret = lsctl->ops->pack_cmd(lsctl, buf, BUF_SZ, cookie);
        free(args);
        ret1E_v((ret < 0), free(buf));
        {
            struct vmsg_usr msg = {
                .addr  = &args->probe_service_rsp_args.from,
                .spec  = NULL,
                .msgId = VMSG_LSCTL,
                .data  = buf,
                .len   = ret
            };
            ret = lsctl->msger.ops->push(&lsctl->msger, &msg);
            ret1E_v((ret < 0), free(buf));
        }
    }
    return ;
}

static
void _aux_vlsctl_iterate_addr_cb2(vsrvcHash* hash, struct sockaddr_in* addr, int last, void* cookie)
{
    union  vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    struct vlsctl* lsctl = args->probe_service_rsp_args.lsctl;
    char* buf = NULL;
    int ret = 0;

    vsockaddr_copy(&args->probe_service_rsp_args.addrs[args->probe_service_rsp_args.index], addr);
    args->probe_service_rsp_args.index++;

    if (last) {
        buf = (char*)malloc(BUF_SZ);
        vlogEv((!buf), elog_malloc);
        ret1E_v((!buf), free(args));
        memset(buf, 0, BUF_SZ);

        ret = lsctl->ops->pack_cmd(lsctl, buf, BUF_SZ, cookie);
        free(args);
        ret1E_v((ret < 0), free(buf));
        {
            struct vmsg_usr msg = {
                .addr  = &args->probe_service_rsp_args.from,
                .spec  = NULL,
                .msgId = VMSG_LSCTL,
                .data  = buf,
                .len   = ret
            };
            ret = lsctl->msger.ops->push(&lsctl->msger, &msg);
            ret1E_v((ret < 0), free(buf));
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
    union vlsctl_rsp_args* args = NULL;
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

    args = (union vlsctl_rsp_args*)malloc(sizeof(*args));
    vlogEv((!args), elog_malloc);
    retE((!args));
    memset(args, 0, sizeof(*args));

    args->probe_service_rsp_args.total = 0;
    args->probe_service_rsp_args.index = 0;
    args->probe_service_rsp_args.pack_cb = lsctl->pack_cmd_ops->probe_service_rsp;
    vtoken_copy(&args->probe_service_rsp_args.hash, &hash);
    memcpy(&args->probe_service_rsp_args.from, from, sizeof(*from));

    ret = app->api_ops->probe_service(app, &hash, _aux_vlsctl_number_addr_cb2, _aux_vlsctl_iterate_addr_cb2, args);
    ret1E((ret < 0), free(args));
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
int _aux_vlsctl_pack_addr(void* buf, int len, struct sockaddr_in* addr)
{
    int tsz = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    tsz += sizeof(uint16_t); // skip family;
    *(uint16_t*)(buf + tsz) = addr->sin_port;
    tsz += sizeof(uint16_t);
    *(uint32_t*)(buf + tsz) = addr->sin_addr.s_addr;
    tsz += sizeof(uint32_t);

    return tsz;
}

static
int _vlsctl_pack_find_service_rsp(void* buf, int len, void* cookie)
{
    union vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;
    int i = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(args);

    *(uint16_t*)(buf + tsz) = VLSCTL_FIND_SERVICE;
    tsz += sizeof(uint16_t);
    *(uint16_t*)(buf + tsz) = 0;
    len_addr = buf + tsz;
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->find_service_rsp_args.hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);
    for (i = 0; i < args->find_service_rsp_args.total; i++) {
        ret = _aux_vlsctl_pack_addr(buf + bsz, len - bsz, &args->find_service_rsp_args.addrs[i]);
        vsockaddr_dump(&args->find_service_rsp_args.addrs[i]);
        printf("\n");
        tsz += ret;
        bsz += ret;
    }
    *(uint16_t*)len_addr = bsz;
    return tsz;
}

static
int _vlsctl_pack_probe_service_rsp(void* buf, int len, void* cookie)
{
    union vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;
    int i = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(args);

    *(uint16_t*)(buf + tsz) = VLSCTL_FIND_SERVICE;
    tsz += sizeof(uint16_t);
    *(uint16_t*)(buf + tsz) = 0;
    len_addr = buf + tsz;
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->probe_service_rsp_args.hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);
    for (i = 0; i < args->probe_service_rsp_args.total; i++) {
        ret = _aux_vlsctl_pack_addr(buf + bsz, len - bsz, &args->find_service_rsp_args.addrs[i]);
        tsz += ret;
        bsz += ret;
    }
    *(uint16_t*)len_addr = bsz;
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

        cid   = *(uint16_t*)(buf + tsz);
        tsz  += sizeof(uint16_t);
        blen -= sizeof(uint16_t);
        clen = *(uint16_t*)(buf + tsz);
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
    union vlsctl_rsp_args* args = (union vlsctl_rsp_args*)cookie;
    void* len_addr = NULL;
    int tsz = 0;
    int ret = 0;

    vassert(lsctl);
    vassert(buf);
    vassert(len > 0);

    *(uint8_t*)(buf + tsz) = VLSCTL_VERSION;
    tsz += sizeof(uint8_t);
    *(uint8_t*)(buf + tsz) = vlsctl_rsp_succ;
    tsz += sizeof(uint8_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    *(uint32_t*)(buf + tsz) = VLSCTL_MAGIC;
    tsz += sizeof(uint32_t);

    ret = args->probe_service_rsp_args.pack_cb(buf+tsz, len-tsz, cookie);
    retE((ret < 0));
    tsz += ret;
    *(uint16_t*)len_addr = (uint16_t)ret;

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

