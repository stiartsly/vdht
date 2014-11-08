#include "vglobal.h"
#include "vnode.h"

#define TICK_INTERVAL ((int)10)

static
char* node_mode_desc[] = {
    "offline",
    "up",
    "running",
    "down",
    "error",
    NULL
};

/*
 * start current dht node.
 * @vnd:
 */
static
int _vnode_start(struct vnode* vnd)
{
    vassert(vnd);

    vlock_enter(&vnd->lock);
    if (vnd->mode != VDHT_OFF) {
        vlock_leave(&vnd->lock);
        return 0;
    }
    vnd->mode = VDHT_UP;
    vlock_leave(&vnd->lock);
    return 0;
}

/*
 * join the given node as dht node.
 * @vnd:
 * @addr:
 */
static
int _vnode_join(struct vnode* vnd, struct sockaddr_in* addr)
{
    vnodeAddr nodeAddr;
    vassert(vnd);
    vassert(addr);

    if (vsockaddr_equal(&vnd->ownId.addr, addr)) {
        return 0;
    }

    vnodeId_make(&nodeAddr.id);
    vsockaddr_copy(&nodeAddr.addr, addr);
    return vnd->route.ops->add(&vnd->route, &nodeAddr, 0);
}

/*
 * @vnd:
 * @addr:
 */
static
int _vnode_drop(struct vnode* vnd, struct sockaddr_in* addr)
{
    vnodeAddr nodeAddr;
    vassert(vnd);
    vassert(addr);

    if (vsockaddr_equal(&vnd->ownId.addr, addr)) {
        return 0;
    }

    vnodeId_make(&nodeAddr.id);
    vsockaddr_copy(&nodeAddr.addr, addr);
    return vnd->route.ops->remove(&vnd->route, &nodeAddr);
}

/*
 * @vnd
 */
static
int _vnode_stop(struct vnode* vnd)
{
    vassert(vnd);

    vlock_enter(&vnd->lock);
    if ((vnd->mode == VDHT_OFF) || (vnd->mode == VDHT_ERR)) {
        vlock_leave(&vnd->lock);
        return 0;
    }
    vnd->mode = VDHT_DOWN;
    vlock_leave(&vnd->lock);
    return 0;
}

static
int _aux_tick_cb(void* cookie)
{
    struct vnode* vnd = (struct vnode*)cookie;
    time_t now = time(NULL);
    int ret = 0;
    vassert(vnd);

    vlock_enter(&vnd->lock);
    switch (vnd->mode) {
    case VDHT_OFF:
    case VDHT_ERR:  {
        //do nothing;
        break;
    }
    case VDHT_UP: {
        ret = vnd->route.ops->load(&vnd->route);
        if (ret < 0) {
            vnd->mode = VDHT_ERR;
            break;
        }
        vnd->ts   = now;
        vnd->mode = VDHT_RUN;
        vlogI(printf("DHT start running"));
        break;
    }
    case VDHT_RUN: {
        if (now - vnd->ts > vnd->tick_interval) {
            vnd->route.ops->tick(&vnd->route);
            vnd->ts = now;
        }
        break;
    }
    case VDHT_DOWN: {
        vnd->route.ops->store(&vnd->route);
        vnd->mode = VDHT_OFF;
        vlogI(printf("DHT become offline"));
        break;
    }
    default:
        vassert(0);
    }
    vlock_leave(&vnd->lock);
    return 0;
}

/*
 * @vnd
 */
static
int _vnode_stabilize(struct vnode* vnd)
{
    struct vticker* ticker = vnd->ticker;
    vassert(vnd);
    vassert(ticker);

    return ticker->ops->add_cb(ticker, _aux_tick_cb, vnd);
}

/*
 * @vnd:
 */
static
int _vnode_dump(struct vnode* vnd)
{
    vassert(vnd);

    vdump(printf("-> NODE"));
    vdump(printf("state:%s", node_mode_desc[vnd->mode]));
    vnd->route.ops->dump(&vnd->route);
    vdump(printf("<- NODE"));
    //todo;

    return 0;
}

static
struct vnode_ops node_ops = {
    .start     = _vnode_start,
    .stop      = _vnode_stop,
    .join      = _vnode_join,
    .drop      = _vnode_drop,
    .stabilize = _vnode_stabilize,
    .dump      = _vnode_dump
//    .get_peers = _vnode_get_peers
};

static
int _aux_get_tick_interval(struct vconfig* cfg)
{
    char buf[32];
    int ret = 0;
    int tms = 0;
    vassert(cfg);

    ret = cfg->ops->get_str(cfg, "node.tick_interval", buf, 32);
    vcall_cond((ret < 0), strcpy(buf, DEF_NODE_TICK_INTERVAL));
    ret = strlen(buf);
    retE((ret <= 0));

    switch(buf[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        retE((1));
    }
    errno = 0;
    ret = strtol(buf, NULL, 10);
    retE((errno));

    return (ret * tms);
}

/*
 * @vnd:
 * @msger:
 * @ticker:
 * @addr:
 */
int vnode_init(struct vnode* vnd, struct vconfig* cfg, struct vmsger* msger, struct vticker* ticker, struct sockaddr_in* addr)
{
    vassert(vnd);
    vassert(cfg);
    vassert(msger);
    vassert(ticker);
    vassert(addr);

    vnodeId_make(&vnd->ownId.id);
    vsockaddr_copy(&vnd->ownId.addr, addr);
    vroute_init(&vnd->route, cfg, msger, &vnd->ownId);

    vlock_init (&vnd->lock);
    vnd->mode  = VDHT_OFF;

    vnd->cfg    = cfg;
    vnd->msger  = msger;
    vnd->ticker = ticker;
    vnd->ops    = &node_ops;

    vnd->tick_interval = _aux_get_tick_interval(cfg);
    retE((vnd->tick_interval < 0));
    return 0;
}

/*
 * @vnd:
 */
void vnode_deinit(struct vnode* vnd)
{
    vassert(vnd);

    vlock_enter(&vnd->lock);
    while (vnd->mode != VDHT_OFF) {
        vlock_leave(&vnd->lock);
        sleep(1);
        vlock_enter(&vnd->lock);
    }
    vlock_leave(&vnd->lock);

    vnd->route.ops->clear(&vnd->route);
    vroute_deinit(&vnd->route);
    vlock_deinit (&vnd->lock);

    return ;
}

