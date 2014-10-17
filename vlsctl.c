#include "vglobal.h"
#include "vlsctl.h"

/* message interface for local service controller.*/

#define BUF_SZ ((int)1024)
#define IS_LSCTL_MAGIC(val) (val == (uint32_t)0x7fec45fa)


/* try to start host.
 *----------------------------
 * lsctl_id -->|
 *----------------------------
 *
 * try to stop host.
 *----------------------------
 * VLSCTL_DHT_DOWN
 *----------------------------
 *
 * try to make host app exit.
 *----------------------------
 * VLSCTL_DHT_SHUTDOWN
 *----------------------------
 *
 * try to add a node with well-known address.
 *------------------------------
 * lsctl-id -->|<-IP->|<-port->|
 *------------------------------
 *
 * try to drop a node with well-known address.
 *------------------------------
 * lsctl-id -->|<-IP->|<-port->|
 *------------------------------
 *
 * try to register replay property.
 *----------------------------
 * VLSCTL_RELAY|
 *----------------------------
 */
static
int _aux_msg_parse_cb(void* cookie, struct vmsg_usr* um)
{
    struct vlsctl* ctl = (struct vlsctl*)cookie;
    struct vhost* host = ctl->host;
    int lsctl_id = 0;
    int ret = 0;

    vassert(ctl);
    vassert(um);

    lsctl_id = get_uint32(um->data);
    switch (lsctl_id) {
    case VLSCTL_DHT_UP:
        ret = host->ops->start(host);
        retE((ret < 0));
        break;

    case VLSCTL_DHT_DOWN:
        ret = host->ops->stop(host);
        retE((ret < 0));
        break;

    case VLSCTL_DHT_EXIT:
        ret = host->ops->req_quit(host);
        retE((ret < 0));
        break;

    case VLSCTL_ADD_NODE: {
        struct sockaddr_in sin;
        void* data = NULL;

        data = offset_addr(um->data, sizeof(uint32_t));
        sin.sin_addr.s_addr = (uint32_t)get_uint32(data);
        data = offset_addr(data, sizeof(uint32_t));
        sin.sin_port   = (short)get_int32(data);
        sin.sin_family = AF_INET;

        ret = host->ops->join(host, &sin);
        retE((ret < 0));
        break;
    }
    case VLSCTL_DEL_NODE: {
        struct sockaddr_in sin;
        void* data = NULL;

        data = offset_addr(um->data, sizeof(uint32_t));
        sin.sin_addr.s_addr = (uint32_t)get_uint32(data);
        data = offset_addr(data, sizeof(uint32_t));
        sin.sin_port   = (short)get_int32(data);
        sin.sin_family = AF_INET;

        ret = host->ops->join(host, &sin);
        retE((ret < 0));
        break;
    }
    case VLSCTL_RELAY_UP:
    case VLSCTL_RELAY_DOWN:
    case VLSCTL_STUN_UP:
    case VLSCTL_STUN_DOWN:
    case VLSCTL_VPN_UP:
    case VLSCTL_VPN_DOWN:
        break;
    default:
        break;
    }
    return 0;
}

static
int _aux_unpack_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    void* data = NULL;
    int  msgId = 0;

    vassert(sm);
    vassert(um);

    retE(!IS_LSCTL_MAGIC(get_uint32(sm->data)));

    msgId = get_int32(offset_addr(sm->data, sizeof(uint32_t)));
    retE((msgId != VMSG_LSCTL));

    data  = offset_addr(sm->data, 8);
    vmsg_usr_init(um, msgId, &sm->addr, sm->len-8, data);
    return 0;
}

int vlsctl_init(struct vlsctl* ctl, struct vhost* host)
{
    struct vconfig* cfg = host->cfg;
    char unix_path[BUF_SZ];
    int ret = 0;

    vassert(ctl);
    vassert(host);

    ret = cfg->ops->get_str(cfg, "lsctl.unix_path", unix_path, BUF_SZ);
    retE((ret < 0));
//    retE((strlen(unix_path) + 1 >= sizeof(ctl->addr.sun_path)));

    ctl->addr.vsun_addr.sun_family = AF_UNIX;
    strcpy(ctl->addr.vsun_addr.sun_path, unix_path);
    ctl->host   = host;
    ctl->waiter = &host->waiter;

    ret += vmsger_init(&ctl->msger);
    ret += vrpc_init(&ctl->rpc, &ctl->msger, VRPC_UNIX, &ctl->addr);
    if (ret < 0) {
        vrpc_deinit(&ctl->rpc);
        vmsger_deinit(&ctl->msger);
        return -1;
    }

    ctl->waiter->ops->add(ctl->waiter, &ctl->rpc);
    vmsger_reg_unpack_cb(&ctl->msger, _aux_unpack_cb, ctl);
    ctl->msger.ops->add_cb(&ctl->msger, ctl, _aux_msg_parse_cb, VMSG_LSCTL);

    return 0;
}

void lsctl_deinit(struct vlsctl* ctl)
{
    vassert(ctl);

    ctl->waiter->ops->remove(ctl->waiter, &ctl->rpc);
    vmsger_deinit(&ctl->msger);
    vrpc_deinit(&ctl->rpc);
    return ;
}
