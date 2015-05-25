#include "vglobal.h"
#include "vroute.h"

struct vconn_probe {
    vtoken    token;
    vnodeId   targetId;
    vnodeConn conn;
    time_t    ts;
};

static MEM_AUX_INIT(conn_probe_cache, sizeof(struct vconn_probe), 0);
struct vconn_probe* vconn_probe_alloc(void)
{
    struct vconn_probe* item = NULL;

    item = (struct vconn_probe*)vmem_aux_alloc(&conn_probe_cache);
    vlogEv((!item), elog_vmem_aux_alloc);
    retE_p((!item));

    memset(item, 0, sizeof(*item));
    return item;
}

static
void vconn_probe_free(struct vconn_probe* item)
{
    vassert(item);
    vmem_aux_free(&conn_probe_cache, item);
    return ;
}

static
int vconn_probe_init(struct vconn_probe* item, vtoken* token, vnodeId* targetId, vnodeConn* conn)
{
    vassert(item);
    vassert(token);
    vassert(conn);

    vtoken_copy(&item->token, token);
    vtoken_copy(&item->targetId, targetId);
    memcpy(&item->conn, conn, sizeof(*conn));
    item->ts = time(NULL);

    return 0;
}

static
int _vroute_conn_space_make(struct vroute_conn_space* space, vtoken* token, vnodeId* targetId, struct vnodeConn* conn)
{
    struct vconn_probe* item = NULL;

    vassert(space);
    vassert(token);
    vassert(targetId);
    vassert(conn);

    item = vconn_probe_alloc();
    retE((!item));
    vconn_probe_init(item, token, targetId, conn);

    varray_add_tail(&space->items, item);
    return 0;
}

static
int _vroute_conn_space_adjust(struct vroute_conn_space* space, vtoken* token)
{
    struct vroute_node_space* node_space = &space->route->node_space;
    struct vconn_probe* item = NULL;
    int found = 1;
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(token);

    for (i = 0; i < varray_size(&space->items); i++) {
        item = (struct vconn_probe*)varray_get(&space->items, i);
        if (vtoken_equal(&item->token, token)) {
            found = 1;
            break;
        }
    }
    if (found) {
        item = (struct vconn_probe*)varray_del(&space->items, i);
        ret = node_space->ops->adjust_connectivity(node_space, &item->targetId, &item->conn);
        vconn_probe_free(item);
    }

    return ret;
}

/*
 * @space:
 */
static
void _vroute_conn_space_timed_reap(struct vroute_conn_space* space)
{
    struct vconn_probe* conn = NULL;
    time_t now = time(NULL);
    int i = 0;
    vassert(space);

    for (i = 0; i < varray_size(&space->items);) {
        conn = (struct vconn_probe*)varray_get(&space->items, i);
        if ((now - conn->ts) > space->max_tmo) {
            varray_del(&space->items, i);
            vconn_probe_free(conn);
        } else {
            i++;
        }
    }
    return ;
}

/*
 * the routine to clear all connect-probe records;
 * @space:
 */
static
void _vroute_conn_space_clear(struct vroute_conn_space* space)
{
    struct vconn_probe* conn = NULL;
    vassert(space);

    while (varray_size(&space->items) > 0) {
        conn = (struct vconn_probe*)varray_pop_tail(&space->items);
        vconn_probe_free(conn);
    }
    return ;
}

/*
 * the routine to dump all connect-probe records;
 *
 * @space:
 */
static
void _vroute_conn_space_dump(struct vroute_conn_space* space)
{
    vassert(space);

    //todo;
    return ;
}


static
struct vroute_conn_space_ops route_conn_space_ops = {
    .make       = _vroute_conn_space_make,
    .adjust     = _vroute_conn_space_adjust,
    .timed_reap = _vroute_conn_space_timed_reap,
    .clear      = _vroute_conn_space_clear,
    .dump       = _vroute_conn_space_dump
};

int vroute_conn_space_init(struct vroute_conn_space* space, struct vroute* route)
{
    vassert(space);
    vassert(route);

    varray_init(&space->items, 4);
    space->route = route;
    space->ops   = &route_conn_space_ops;

    return 0;
}

void vroute_conn_space_deinit(struct vroute_conn_space* space)
{
    vassert(space);

    space->ops->clear(space);
    varray_deinit(&space->items);

    return ;
}

