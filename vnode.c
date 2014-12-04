#include "vglobal.h"
#include "vnode.h"

enum vnode_mode {
    VDHT_OFF,
    VDHT_UP,
    VDHT_RUN,
    VDHT_DOWN,
    VDHT_ERR,
    VDHT_M_BUTT
};

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
    struct vroute* route = vnd->route;
    vnodeAddr node_addr;
    int ret = 0;

    vassert(vnd);
    vassert(addr);

    if (vsockaddr_equal(&vnd->ownId.addr, addr)) {
        return 0;
    }

    vtoken_make(&node_addr.id);
    vsockaddr_copy(&node_addr.addr, addr);
    ret = route->ops->join_node(route, &node_addr);
    retE((ret < 0));
    return 0;
}

/*
 * @vnd:
 * @addr:
 */
static
int _vnode_drop(struct vnode* vnd, struct sockaddr_in* addr)
{
    struct vroute* route = vnd->route;
    vnodeAddr node_addr;
    int ret = 0;

    vassert(vnd);
    vassert(addr);

    if (vsockaddr_equal(&vnd->ownId.addr, addr)) {
        return 0;
    }

    vtoken_make(&node_addr.id);
    vsockaddr_copy(&node_addr.addr, addr);
    ret = route->ops->drop_node(route, &node_addr);
    retE((ret < 0));
    return 0;
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
int _aux_node_tick_cb(void* cookie)
{
    struct vnode* vnd = (struct vnode*)cookie;
    time_t now = time(NULL);
    vassert(vnd);

    vlock_enter(&vnd->lock);
    switch (vnd->mode) {
    case VDHT_OFF:
    case VDHT_ERR:  {
        //do nothing;
        break;
    }
    case VDHT_UP: {
        if (vnd->route->ops->load(vnd->route) < 0) {
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
            vnd->route->ops->tick(vnd->route);
            vnd->ts = now;
        }
        break;
    }
    case VDHT_DOWN: {
        vnd->route->ops->store(vnd->route);
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
    int ret = 0;

    vassert(vnd);
    vassert(ticker);

    ret = ticker->ops->add_cb(ticker, _aux_node_tick_cb, vnd);
    retE((ret < 0));
    return 0;
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
    vdump(printf("<- NODE"));

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
};

/*
 * @vnd:
 * @msger:
 * @ticker:
 * @addr:
 */
int vnode_init(struct vnode* vnd, struct vconfig* cfg, struct vticker* ticker, struct vroute* route, vnodeAddr* node_addr)
{
    int ret = 0;

    vassert(vnd);
    vassert(cfg);
    vassert(ticker);
    vassert(node_addr);

    vnodeAddr_copy(&vnd->ownId, node_addr);
    vlock_init(&vnd->lock);
    vnd->mode  = VDHT_OFF;

    vnd->cfg    = cfg;
    vnd->ticker = ticker;
    vnd->route  = route;
    vnd->ops    = &node_ops;

    ret = cfg->inst_ops->get_node_tick_interval(cfg, &vnd->tick_interval);
    retE((ret < 0));
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
    vlock_deinit (&vnd->lock);

    return ;
}

