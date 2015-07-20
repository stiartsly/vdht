#include "vglobal.h"
#include "vroute.h"

struct vticket_item {
    int ts;
    int type;

    union ticket_info {
        /* used to check wheather DHT response is the valid, which means the
         * response is acked for query sent from my DHT node.
         */
        struct nonce_check_info {
            vnonce nonce;
        } check;

        /*
         * used to
         */
        struct srvc_probe_info {
            vsrvcHash hash;
            vsrvcInfo_number_addr_t  ncb;
            vsrvcInfo_iterate_addr_t icb;
            void* cookie;
        } srvc;

        /*
         * used to
         */
        struct conn_probe_info {
            vnonce    nonce;
            vnodeId   targetId;
            vnodeConn conn;
            void* cookie;
        } conn;
    } info;
};

static MEM_AUX_INIT(route_ticket_cache, sizeof(struct vticket_item), 4);
static
struct vticket_item* vticket_alloc(void)
{
    struct vticket_item* ticket = NULL;

    ticket = (struct vticket_item*)vmem_aux_alloc(&route_ticket_cache);
    vlogEv((!ticket), elog_vmem_aux_alloc);
    retE_p((!ticket));
    memset(ticket, 0, sizeof(*ticket));
    return ticket;
}

static
void vticket_free(struct vticket_item* ticket)
{
    vassert(ticket);
    vmem_aux_free(&route_ticket_cache, ticket);
    return ;
}

static
void vticket_init(struct vticket_item* ticket, int type, void** argv)
{
    vassert(ticket);
    vassert(type >= 0);
    vassert(type < VTICKET_BUTT);
    vassert(ticket);

    ticket->type = type;
    ticket->ts   = time(NULL);

    switch(type) {
    case VTICKET_TOKEN_CHECK: {
        struct nonce_check_info* info = &ticket->info.check;
        /* argv = {
         *     nonce
         * }
         */
        varg_decl(argv, 0, vnonce*, nonce);
        vnonce_copy(&info->nonce, nonce);
        break;
    }
    case VTICKET_SRVC_PROBE: {
        struct srvc_probe_info* info = &ticket->info.srvc;
        /* argv[] = {
         *     hash,
         *     ncb,
         *     icb,
         *     cookie
         * }
         */
        varg_decl(argv, 0, vsrvcHash*, hash);
        varg_decl(argv, 1, vsrvcInfo_number_addr_t,  ncb);
        varg_decl(argv, 2, vsrvcInfo_iterate_addr_t, icb);
        varg_decl(argv, 3, void*, cookie);

        vtoken_copy(&info->hash, hash);
        info->ncb = ncb;
        info->icb = icb;
        info->cookie = cookie;
        break;
    }
    case VTICKET_CONN_PROBE: {
        struct conn_probe_info* info = &ticket->info.conn;
        /* argv[] = {
         *        nonce,
         *        targetId,
         *        conn,
         *        cookie
         * }
         */
        varg_decl(argv, 0, vnonce*, nonce);
        varg_decl(argv, 1, vnodeId*, targetId);
        varg_decl(argv, 2, vnodeConn*, conn);
        varg_decl(argv, 3, void*, cookie);

        vnonce_copy(&info->nonce, nonce);
        vtoken_copy(&info->targetId, targetId);
        memcpy(&info->conn, conn, sizeof(*conn));
        info->cookie = cookie;
        break;
    }
    default:
        vassert(0);
        break;
    }
    return;
}

static
void vticket_dump(struct vticket_item* ticket)
{
    vassert(ticket);
    //todo;
    return ;
}

/*
 * punch a ticket according to userdata @argv
 * @ticket:
 * @argv: an array of userdata;
 */
static
int vticket_punch(struct vticket_item* ticket, void** argv)
{
    int done = 0;

    vassert(ticket);
    vassert(argv);

    switch(ticket->type) {
    case VTICKET_TOKEN_CHECK: {
        struct nonce_check_info* info = &ticket->info.check;
        /*
         * argv[] = {
         *     nonce
         * }
         */
        varg_decl(argv, 0, vnonce*, nonce);
        if (vnonce_equal(&info->nonce, nonce)) {
            done = 1;
        }
        break;
    }
    case VTICKET_SRVC_PROBE: {
        struct srvc_probe_info* info = &ticket->info.srvc;
        /*
         * argv[] = {
         *     srvci
         * }
         */
        varg_decl(argv, 0, vsrvcInfo*, srvci);
        int i = 0;
        if (vtoken_equal(&info->hash, &srvci->hash)) {
            info->ncb(&srvci->hash, srvci->naddrs, srvci->proto, info->cookie);
            for (i = 0; i < srvci->naddrs; i++) {
                info->icb(&srvci->hash, &srvci->addrs[i].addr, srvci->addrs[i].type, (i+1)== srvci->naddrs, info->cookie);
            }
            done = 1;
        }
        break;
    }
    case VTICKET_CONN_PROBE: {
        struct conn_probe_info*   info  = &ticket->info.conn;
        struct vroute_node_space* space = (struct vroute_node_space*)info->cookie;
        /*
         * argv[] = {
         *      nonce
         * }
         */
        varg_decl(argv, 0, vnonce*, nonce);

        if (vnonce_equal(&info->nonce, nonce)) {
            space->ops->adjust_connectivity(space, &info->targetId, &info->conn);
            done = 1;
        }
        break;
    }
    default:
        vassert(0);
        break;
    }
    return done;
}

static
void vticket_timedout_punch(struct vticket_item* ticket)
{
    vassert(ticket);

    switch(ticket->type) {
    case VTICKET_TOKEN_CHECK:
        break;
    case VTICKET_SRVC_PROBE: {
        struct srvc_probe_info* info = &ticket->info.srvc;
        info->ncb(&info->hash, 0, VPROTO_UNKNOWN, info->cookie);
        break;
    }
    case VTICKET_CONN_PROBE:
        break;
    default:
        vassert(0);
        break;
    }
    return ;
}

/*
 * the routine to add a ticket
 * @ticket:
 * @type :  ticket type;
 * @argv :  an array of pointers to user data;
 */
static
int _vroute_tckt_space_add_ticket(struct vroute_tckt_space* space, int type, void** argv)
{
    struct vticket_item* ticket = NULL;
    vassert(space);
    vassert(type >= 0);
    vassert(type < VTICKET_BUTT);
    vassert(argv);

    ticket = vticket_alloc();
    retE((!ticket));
    vticket_init(ticket, type, argv);
    varray_add_tail(&space->tickets, ticket);
    return 0;
}

/*
 * the routine to check connent(@argv) is valid by iterate all tickets, and
 * if find a correspond ticket, then punch and clear it.
 */
static
int _vroute_tckt_space_check_ticket(struct vroute_tckt_space* space, int type, void** argv)
{
    struct vticket_item* ticket = NULL;
    int found = 0;
    int ret = 0;
    int i = 0;

    vassert(space);
    vassert(type >= 0);
    vassert(type < VTICKET_BUTT);
    vassert(argv);

    for (i = 0; i < varray_size(&space->tickets);) {
        ticket = (struct vticket_item*)varray_get(&space->tickets, i);
        if (ticket->type != type) {
            i++;
            continue;
        }
        ret = vticket_punch(ticket, argv);
        if (ret > 0) { // found and handled.
            varray_del(&space->tickets, i);
            vticket_free(ticket);
            found = 1;
        } else {
            i++;
            continue;
        }
        if (type == VTICKET_TOKEN_CHECK) {
            break;
        }
    }
    return found;
}

/*
 * the routine to reap all expired tickets
 * @space:
 */
static
void _vroute_tckt_space_timed_reap(struct vroute_tckt_space* space)
{
    struct vticket_item* ticket = NULL;
    time_t now = time(NULL);
    int i = 0;
    vassert(space);

    for (i = 0; i < varray_size(&space->tickets); ) {
        ticket = (struct vticket_item*)varray_get(&space->tickets, i);
        if (now - ticket->ts >= space->overdue_tmo) {
            vticket_timedout_punch(ticket);
            varray_del(&space->tickets, i);
            vticket_free(ticket);
        } else {
            i++;
        }
    }
    return ;
}


/*
 * the routine to clear all tickets.
 * @space:
 */
static
void _vroute_tckt_space_clear(struct vroute_tckt_space* space)
{
    struct vticket_item* ticket = NULL;
    vassert(space);

    while (varray_size(&space->tickets) > 0) {
        ticket = (struct vticket_item*)varray_pop_tail(&space->tickets);
        vticket_free(ticket);
    }
    return ;
}


/*
 * the routine to dump ticket;
 * @space:
 */
static
void _vroute_tckt_space_dump(struct vroute_tckt_space* space)
{
    struct vticket_item* ticket = NULL;
    int i = 0;
    vassert(space);

    for (i = 0; i < varray_size(&space->tickets); i++) {
        ticket = (struct vticket_item*)varray_get(&space->tickets, i);
        vticket_dump(ticket);
    }
    return ;
}

static
struct vroute_tckt_space_ops route_ticket_space_ops = {
    .add_ticket   = _vroute_tckt_space_add_ticket,
    .check_ticket = _vroute_tckt_space_check_ticket,
    .timed_reap   = _vroute_tckt_space_timed_reap,
    .clear        = _vroute_tckt_space_clear,
    .dump         = _vroute_tckt_space_dump
};


int vroute_tckt_space_init(struct vroute_tckt_space* space)
{
    vassert(space);

    space->overdue_tmo = 5; //5 seconds
    varray_init(&space->tickets, 8);

    space->ops = &route_ticket_space_ops;
    return 0;
}

void vroute_tckt_space_deinit(struct vroute_tckt_space* space)
{
    vassert(space);

    space->ops->clear(space);
    varray_deinit(&space->tickets);
    return ;
}

