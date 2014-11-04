#include "vglobal.h"
#include "vlsctl.h"

/* message interface for local service controller.*/
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
        char ip[64];
        int  port = 0;
        void* data = NULL;

        data = offset_addr(um->data, sizeof(uint32_t));
        port = get_int32(data);
        data = offset_addr(data, sizeof(long));
        memset(ip, 0, 64);
        strcpy(ip, data);
        ret = vsockaddr_convert(ip, port, &sin);
        retE((ret < 0));
        ret = host->ops->join(host, &sin);
        retE((ret < 0));
        vlogI(printf("host join a node(%s:%d)", ip, port));
        break;
    }
    case VLSCTL_DEL_NODE: {
        struct sockaddr_in sin;
        char ip[64];
        int  port = 0;
        void* data = NULL;

        data = offset_addr(um->data, sizeof(uint32_t));
        port = get_int32(data);
        data = offset_addr(data, sizeof(long));
        memset(ip, 0, 64);
        strcpy(ip, data);
        ret = vsockaddr_convert(ip, port, &sin);
        retE((ret < 0));
        ret = host->ops->drop(host, &sin);
        retE((ret < 0));
        vlogI(printf("host drop a node(%s:%d)", ip, port));
        break;
    }
    case VLSCTL_RELAY_UP:
        vlogI(printf("relay up"));
        //todo;
        break;
    case VLSCTL_RELAY_DOWN:
        vlogI(printf("relay down"));
        //todo;
        break;
    case VLSCTL_STUN_UP:
        vlogI(printf("stun up"));
        //todo;
        break;
    case VLSCTL_STUN_DOWN:
        vlogI(printf("stun down"));
        //todo;
        break;
    case VLSCTL_VPN_UP:
        vlogI(printf("vpn up"));
        //todo;
        break;
    case VLSCTL_VPN_DOWN:
        vlogI(printf("vpn down"));
        //todo;
        break;
    default:
        break;
    }
    return 0;
}

static
int _aux_msg_unpack_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    uint32_t magic = 0;
    void* data = sm->data;
    int  msgId = 0;
    int  sz = 0;

    vassert(sm);
    vassert(um);

    magic = get_uint32(data);
    retE((!IS_LSCTL_MAGIC(magic)));
    data = offset_addr(data, sizeof(uint32_t));
    sz += sizeof(uint32_t);

    msgId = get_int32(data);
    retE((msgId != VMSG_LSCTL));
    data = offset_addr(data, sizeof(long));
    sz += sizeof(long);

    vmsg_usr_init(um, msgId, &sm->addr, sm->len-sz, data);
    return 0;
}

int vlsctl_init(struct vlsctl* ctl, struct vhost* host)
{
    struct vconfig_ops* ops = host->cfg->ops;
    struct sockaddr_un* sun = &ctl->addr.vsun_addr;
    char unix_path[BUF_SZ];
    int ret = 0;

    vassert(ctl);
    vassert(host);

    memset(unix_path, 0, BUF_SZ);
    ret = ops->get_str(host->cfg, "lsctl.unix_path", unix_path, BUF_SZ);
    retE((ret < 0));
    retE((strlen(unix_path) + 1 >= sizeof(sun->sun_path)));

    sun->sun_family = AF_UNIX;
    strcpy(sun->sun_path, unix_path);
    ctl->host = host;

    ret += vmsger_init(&ctl->msger);
    ret += vrpc_init(&ctl->rpc, &ctl->msger, VRPC_UNIX, &ctl->addr);
    if (ret < 0) {
        vrpc_deinit(&ctl->rpc);
        vmsger_deinit(&ctl->msger);
        return -1;
    }

    vmsger_reg_unpack_cb  (&ctl->msger, _aux_msg_unpack_cb, ctl);
    ctl->msger.ops->add_cb(&ctl->msger, ctl, _aux_msg_parse_cb, VMSG_LSCTL);
    return 0;
}

void vlsctl_deinit(struct vlsctl* ctl)
{
    vassert(ctl);

    vmsger_deinit(&ctl->msger);
    vrpc_deinit(&ctl->rpc);
    return ;
}

