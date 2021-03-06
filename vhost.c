#include "sqlite3.h"
#include "vglobal.h"
#include "vhost.h"

/*
 * the routine to start host asynchronizedly.
 * @host: handle to host.
 */
static
int _vhost_start(struct vhost* host)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);

    ret = node->ops->start(node);
    retE((ret < 0));
    vlogI("host started");
    return 0;
}

/*
 * the routine to stop host asynchronizedly.
 * @host:
 */
static
int _vhost_stop(struct vhost* host)
{
    struct vnode* node = &host->node;
    int ret = 0;
    vassert(host);

    ret = node->ops->stop(node);
    retE((ret < 0));
    vlogI("host stopped");
    return 0;
}

/*
 * the routine to add wellknown node to route table.
 * @host:
 * @wellknown_addr: address of node to join.
 */
static
int _vhost_join(struct vhost* host, struct sockaddr_in* wellknown_addr)
{
    struct vroute* route = &host->route;
    struct vnode*  node  = &host->node;
    int ret = 0;

    vassert(host);
    vassert(wellknown_addr);

    if (node->ops->has_addr(node, wellknown_addr)) {
        return 0;
    }
    ret = route->ops->join_node(route, wellknown_addr);
    retE((ret < 0));
    vlogI("join a node");
    return 0;
}

/*
 * the routine to register the periodical work with certian interval
 * to the ticker, which is running peridically at background in the
 * form of timer.
 * @host:
 */
static
int _vhost_stabilize(struct vhost* host)
{
    struct vticker* ticker = &host->ticker;
    struct vnode*    node  = &host->node;
    int ret = 0;

    vassert(host);

    ret = node->ops->stabilize(node);
    retE((ret < 0));
    ret = ticker->ops->start(ticker, host->tick_tmo);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to dump all infomation about host.
 * @host:
 */
static
void _vhost_dump(struct vhost* host)
{
    vassert(host);

    vdump(printf("-> HOST"));
    host->node.ops->dump(&host->node);
    host->route.ops->dump(&host->route);
    host->waiter.ops->dump(&host->waiter);
    vdump(printf("<- HOST"));

    return;
}

/*
 * the routine to reqeust to quit from laundry loop and wait for
 * thread to quit.
 * @host:
 */
static
int _vhost_exit(struct vhost* host)
{
    vassert(host);

    host->to_quit = 1;
    host->node.ops->stop(&host->node);
    host->node.ops->wait_for_stop(&host->node);

    vlogI("host exited");
    return 0;
}

/*
 * todo:
 * @host:
 */
static
int _vhost_run(struct vhost* host)
{
    struct vwaiter* wt = &host->waiter;
    int ret = 0;
    vassert(host);

    host->to_quit = 0;

    vlogI("host laundrying");
    while(!host->to_quit) {
        ret = wt->ops->laundry(wt);
        if (ret < 0) {
            continue;
        }
    }
    vlogI("host quited from laundrying");
    return 0;
}

/* the routine to get version
 * @host:
 * @buf : buffer to store the version.
 * @len : buffer length.
 */
static
char* _vhost_version(struct vhost* host)
{
    vassert(host);
    return (char*)vhost_get_version();
}

/*
 * the routine to send bogus query to dest node with give address. It only
 * can be used as debug usage.
 * @host:
 * @what: what dht query;
 * @dest: dest address of dht query.
 */
static
int _vhost_bogus_query(struct vhost* host, int what, struct sockaddr_in* remote_addr)
{
    struct vroute* route = &host->route;
    vnodeConn conn;
    int ret = 0;

    vassert(host);
    vassert(remote_addr);

    vnodeConn_set_raw(&conn, &host->zaddr, remote_addr);
    switch(what) {
    case VDHT_PING:
        ret = route->dht_ops->ping(route, &conn);
        break;
    case VDHT_FIND_NODE:
        ret = route->dht_ops->find_node(route, &conn, &host->myid);
        break;
    case VDHT_FIND_CLOSEST_NODES:
        ret = route->dht_ops->find_closest_nodes(route, &conn, &host->myid);
        break;
    case VDHT_REFLEX:
        ret = route->dht_ops->reflex(route, &conn);
        break;
    case VDHT_POST_SERVICE:
    default:
        retE((1));
        break;
    }
    retE((ret < 0));
    return 0;
}

static
struct vhost_ops host_ops = {
    .start         = _vhost_start,
    .stop          = _vhost_stop,
    .join          = _vhost_join,
    .stabilize     = _vhost_stabilize,
    .exit          = _vhost_exit,
    .run           = _vhost_run,
    .dump          = _vhost_dump,
    .version       = _vhost_version,
    .bogus_query   = _vhost_bogus_query
};

/*
 * @cookie
 * @um: [in]  usr msg format
 * @sm: [out] sys msg format
 *
 * 1. dht msg format (usr msg is same as sys msg.)
 * ----------------------------------------------------------
 * |<- dht magic ->|<- msgId ->|<-- dht msg content. -----|
 * ----------------------------------------------------------
 *
 * 2. other msg format
 *
 */
static
int _aux_vhost_pack_msg_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys* sm)
{
    char* data = NULL;
    int sz = 0;

    vassert(cookie);
    vassert(um);
    vassert(sm);

    if (VDHT_MSG(um->msgId)) {
        sz += sizeof(int32_t);
        data = unoff_addr(um->data, sz);
        set_int32(data, um->msgId);

        sz += sizeof(uint32_t);
        data = unoff_addr(um->data, sz);
        set_uint32(data, DHT_MAGIC);

        vmsg_sys_init(sm, um->addr, um->spec, um->len + sz, data);
        return 0;
    }

    //todo: if needed, add other message type processing routine here.
    return -1;
}

/*
 * @cookie:
 * @sm:[in] sys msg format
 * @um:[out]usr msg format
 *
 */
static
int _aux_vhost_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    void* data = sm->data;
    uint32_t magic = 0;
    int msgId = 0;
    int sz = 0;

    vassert(cookie);
    vassert(sm);
    vassert(um);

    magic = get_uint32(data);
    sz += sizeof(uint32_t);

    data = offset_addr(sm->data, sz);
    msgId = get_int32(data);
    sz += sizeof(int32_t);

    data = offset_addr(sm->data, sz);
    if (IS_DHT_MSG(magic)) {
        vmsg_usr_init(um, msgId, &sm->addr, &sm->spec, sm->len -sz, data);
        return 0;
    }

    //todo: if needed, add other message type processing routine here.
    return 0;
}

static
int _aux_vhost_get_nodeId_cb(void* priv, int col, char** value, char** field)
{
    char* sid = (char*)((void**)value)[0];
    vnodeId* myid = (vnodeId*)priv;
    vassert(myid);

    vtoken_unstrlize(sid, myid);
    return 0;
}

static
int _aux_vhost_get_nodeId(struct vconfig* cfg, vnodeId* myid)
{
#define VNODEID_TB  ((const char*)"dht_public_id")
    sqlite3* db = NULL;
    char* err = NULL;
    char buf[BUF_SZ];
    char sid[64];
    int ret = 0;

    strncpy(buf, cfg->ext_ops->get_host_db_file(cfg), BUF_SZ);
    ret = sqlite3_open(buf, &db);
    vlogEv((ret), elog_sqlite3_open);
    retE((ret));

    memset(buf, 0, BUF_SZ);
    sprintf(buf, "select * from '%s'", VNODEID_TB);
    ret = sqlite3_exec(db, buf, _aux_vhost_get_nodeId_cb, myid, &err);
    if (!ret) {
        sqlite3_close(db);
        return 0;
    }

    memset(buf, 0, BUF_SZ);
    sprintf(buf, "CREATE TABLE '%s' (id char(160))", VNODEID_TB);
    ret = sqlite3_exec(db, buf, 0, 0, &err);
    vlogEv((ret), elog_sqlite3_exec);
    vlogEv((ret && err), "create db err:%s\n", err);
    retE(ret);

    vtoken_make(myid);
    memset(sid, 0, 64);
    vtoken_strlize(myid, sid, 64);

    memset(buf, 0, BUF_SZ);
    sprintf(buf, "insert into '%s' (id) values ('%s')", VNODEID_TB, sid);
    ret = sqlite3_exec(db, buf, 0, 0, &err);
    vlogEv((ret), elog_sqlite3_exec);
    vlogEv((ret && err), "insert elem err:%s.\n", err);
    retE((ret));

    sqlite3_close(db);
    return 0;
}

int vhost_init(struct vhost* host, struct vconfig* cfg, struct vlsctl* lsctl)
{
    int ret = 0;

    vassert(host);
    vassert(cfg);
    memset(host, 0, sizeof(*host));

    host->tick_tmo = cfg->ext_ops->get_host_tick_tmo(cfg);
    host->to_quit  = 0;
    host->cfg      = cfg;
    host->lsctl    = lsctl;
    host->ops      = &host_ops;

    ret = cfg->ext_ops->get_dht_address(cfg, &host->zaddr);
    retE((ret < 0));
    ret = _aux_vhost_get_nodeId(cfg, &host->myid);
    retE((ret < 0));

    ret += vticker_init(&host->ticker);
    ret += vwaiter_init(&host->waiter);
    ret += vmsger_init (&host->msger);
    ret += vrpc_init   (&host->rpc,  &host->msger, VRPC_UDP, to_vsockaddr_from_sin(&host->zaddr));
    ret += vroute_init (&host->route, cfg, host, &host->myid);
    ret += vnode_init  (&host->node,  cfg, host, &host->myid);
    if (ret < 0) {
        vnode_deinit   (&host->node);
        vroute_deinit  (&host->route);
        vrpc_deinit    (&host->rpc);
        vmsger_deinit  (&host->msger);
        vwaiter_deinit (&host->waiter);
        vticker_deinit (&host->ticker);
        return -1;
    }

    host->waiter.ops->add(&host->waiter, &host->rpc);
    host->waiter.ops->add(&host->waiter, &lsctl->rpc);
    vmsger_reg_pack_cb  (&host->msger, _aux_vhost_pack_msg_cb  , host);
    vmsger_reg_unpack_cb(&host->msger, _aux_vhost_unpack_msg_cb, host);

    return 0;
}

void vhost_deinit(struct vhost* host)
{
    vassert(host);

    host->waiter.ops->remove(&host->waiter, &host->lsctl->rpc);
    host->waiter.ops->remove(&host->waiter, &host->rpc);

    vnode_deinit  (&host->node);
    vroute_deinit (&host->route);
    vrpc_deinit   (&host->rpc);
    vwaiter_deinit(&host->waiter);
    vticker_deinit(&host->ticker);
    vmsger_deinit (&host->msger);

    return;
}

const char* vhost_get_version(void)
{
    /*
     * version : a.b.c.d.e
     * a: major
     * b: minor
     * c: bugfix
     * d: demo(candidate)
     * e: skipped.
     */
    const char* version = "0.0.0.1.0";
    return (const char*)version;
}

