#include "vglobal.h"
#include "vupnpc.h"
#include "miniupnpc.h"
#include "upnpcommands.h"

struct vupnpc_config {
    struct UPNPDev* devlist;
    struct UPNPUrls urls;
    struct IGDdatas data;
    char lan_addr[32];
};

static struct vupnpc_config upnpc_config;
static
const char* upnp_protos[] = {
    "UDP",
    "TCP",
    0
};

static
void _aux_dump_IGD_devices(struct UPNPDev* devlist)
{
    struct UPNPDev* dev = NULL;
    vassert(devlist);

    vdump(printf("-> List of UPNP devices found:"));
    for (dev = devlist; dev; dev = dev->pNext) {
        vdump(printf("desc:%s", dev->descURL));
        vdump(printf("st:%s",   dev->st));
    }
    vdump(printf("<-"));
    return ;
}
/*
 * the routine to setup up stuffs before mapping port
 *
 * @upnpc:
 */
static
int _vupnpc_setup(struct vupnpc* upnpc)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    int ret = 0;
    vassert(upnpc);

    retE((UPNPC_NOIGD != upnpc->state && UPNPC_RAW != upnpc->state));

    memset(cfg, 0, sizeof(*cfg));
    cfg->devlist = upnpDiscover(2000, NULL, NULL, 0, 0, NULL);
    if (!cfg->devlist) {
        elog_upnpDiscover;
        vlogI(printf("No IGD found"));
        upnpc->state = UPNPC_NOIGD;
        return -1;
    }
    _aux_dump_IGD_devices(cfg->devlist);
    ret = UPNP_GetValidIGD(cfg->devlist, &cfg->urls, &cfg->data, cfg->lan_addr, 32);
    if (ret < 0) {
        elog_UPNP_GetValidIGD;
        vlogI(printf("No IGD found"));
        freeUPNPDevlist(cfg->devlist);
        upnpc->state = UPNPC_NOIGD;
        return -1;
    }
    vsockaddr_convert(cfg->lan_addr, upnpc->iport, &upnpc->iaddr);
    upnpc->state = UPNPC_READY;
    return 0;
}

/*
 * the routine to shutdown upnpc
 *
 * @upnpc
 */
static
int _vupnpc_shutdown(struct vupnpc* upnpc)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    vassert(upnpc);

    switch(upnpc->state) {
    case UPNPC_ACTIVE:
        upnpc->act_ops->unmap_port(upnpc);
        // fall through next case.
    case UPNPC_READY:
        freeUPNPDevlist(cfg->devlist);
        upnpc->state = UPNPC_RAW;
        break;

    case UPNPC_RAW:
        break;
    case UPNPC_NOIGD:
    case UPNPC_ERR:
    default:
        upnpc->state = UPNPC_RAW;
        break;
    }
    return 0;
}

/*
 * the routine to map a port to IGD
 *
 * @upnpc:
 * @iport:
 * @eport:
 * @proto:
 * @eaddr:
 */
static
int _vupnpc_map_port(struct vupnpc* upnpc)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    char siaddr[32];
    char seaddr[32];
    char siport[8];
    char seport[8];
    char sproto[8];
    char sduration[8];
    char siaddr_tmp[32];
    char siport_tmp[8];
    char tmp[64];
    int  ret = 0;

    vassert(upnpc);

    memset(siaddr, 0, 32);
    memset(seaddr, 0, 32);
    memset(siport, 0, 8);
    memset(seport, 0, 8);
    memset(sproto, 0, 8);
    memset(sduration,  0, 8);
    memset(siaddr_tmp, 0, 32);
    memset(siport_tmp, 0, 8);

    {
        uint32_t uiaddr = ntohl(upnpc->iaddr.sin_addr.s_addr);
        snprintf(siaddr, 32, "%d.%d.%d.%d",
                (uiaddr >> 24) & 0xff,
                (uiaddr >> 16) & 0xff,
                (uiaddr >> 8 ) & 0xff,
                (uiaddr >> 0 ) & 0xff);
    }
    snprintf(siport, 8, "%d", upnpc->iport);
    snprintf(seport, 8, "%d", upnpc->eport);
    snprintf(sproto, 8, "%s", upnp_protos[upnpc->proto]);

    retE((UPNPC_READY != upnpc->state));
    ret = UPNP_GetExternalIPAddress(cfg->urls.controlURL, cfg->data.first.servicetype, seaddr);
    vlog((ret < 0), elog_UPNP_GetExternalIPAddress);
    retE((ret < 0));
    vlog((!seaddr[0]), vlogI(printf("External address is empty")));
    retE((!seaddr[0]));

    vlogI(printf("External address: %s", seaddr));

    memset(tmp, 0, 64);
    ret = UPNP_AddPortMapping(cfg->urls.controlURL,
                cfg->data.first.servicetype,
                seport, siport,
                siaddr, "aaaa",
                sproto, 0,
                "0");
    vlog((ret < 0), elog_UPNP_AddPortMapping);
    retE((ret < 0));


    ret = UPNP_GetSpecificPortMappingEntry(cfg->urls.controlURL,
                cfg->data.first.servicetype,
                seport, sproto, NULL,
                siaddr_tmp, siport_tmp,
                NULL, NULL, tmp);
    vlog((ret < 0), elog_UPNP_GetSpecificPortMappingEntry);
    retE((ret < 0));
    printf("<%s> siaddr_tmp:%s\n", __FUNCTION__, siaddr_tmp);
    vlog((!siaddr_tmp[0]), vlogI(printf("Local address is empty")));
    retE((!siaddr_tmp[0]));

    if (strcmp(siaddr, siaddr_tmp) || strcmp(siport, siport_tmp)) {
        vlogI(printf("Local address is not same"));
        return -1;
    }
    vlogI(printf("Local addr:%s:%s", siaddr, siport));
    vsockaddr_convert(seaddr, upnpc->eport, &upnpc->eaddr);
    upnpc->state = UPNPC_ACTIVE;
    vlogI(printf("uPnP Forward/Mapping Succeeded"));
    return 0;
}

/*
 * the routine to unmap port to IGD.
 *
 * @upnpc
 * @ext_port:
 * @proto
 */
static
int _vupnpc_unmap_port(struct vupnpc* upnpc)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    char seport[8];
    char sproto[8];
    int ret = 0;

    vassert(upnpc);
    memset(seport, 0, 8);
    memset(sproto, 0, 8);

    snprintf(seport, 8, "%d", upnpc->old_eport);
    snprintf(sproto, 8, "%s", upnp_protos[upnpc->proto]);

    vlock_enter(&upnpc->lock);
    retE((UPNPC_ACTIVE != upnpc->state));

    ret = UPNP_DeletePortMapping(cfg->urls.controlURL,
                cfg->data.first.servicetype,
                seport, sproto, NULL);
    vlog((ret < 0), elog_UPNP_DeletePortMapping);
    retE((ret < 0));

    upnpc->state = UPNPC_READY;
    return 0;
}

/*
 * the routine to get status from IGD
 *
 * @upnpc
 * @status:
 */
static
int _vupnpc_get_status(struct vupnpc* upnpc, struct vupnpc_status* status)
{
    vassert(upnpc);
    vassert(status);

    //todo;
    return 0;
}

/*
 * the routine to dump all mapping address infomation
 *
 * @upnpc
 */
static
void _vupnpc_dump_mapping_port(struct vupnpc* upnpc)
{
    vassert(upnpc);

    //todo;
    return ;
}

static
struct vupnpc_act_ops upnpc_act_ops = {
    .setup      = _vupnpc_setup,
    .shutdown   = _vupnpc_shutdown,
    .map_port   = _vupnpc_map_port,
    .unmap_port = _vupnpc_unmap_port,
    .get_status = _vupnpc_get_status,
    .dump_mapping_port = _vupnpc_dump_mapping_port
};

static
int _aux_upnpc_thread_entry(void* argv)
{
    struct vupnpc* upnpc = (struct vupnpc*)argv;
    vassert(upnpc);

    vlock_enter(&upnpc->lock);
    while(!upnpc->to_quit) {
        vcond_wait(&upnpc->cond, &upnpc->lock);
        switch(upnpc->state) {
        case UPNPC_RAW:
            vlock_leave(&upnpc->lock);
            upnpc->act_ops->setup(upnpc);
            //fall throught to next.
        case UPNPC_READY:
            upnpc->act_ops->map_port(upnpc);
            break;
        case UPNPC_ACTIVE:
            vlock_leave(&upnpc->lock);
            upnpc->act_ops->unmap_port(upnpc);
            upnpc->act_ops->map_port(upnpc);
            break;
        default:
            //todo;
            break;
        }
        vlock_enter(&upnpc->lock);
    }
    vlock_leave(&upnpc->lock);
    upnpc->act_ops->unmap_port(upnpc);
    upnpc->act_ops->shutdown(upnpc);
    return 0;
}

static
int _vupnpc_start(struct vupnpc* upnpc)
{
    int ret = 0;
    vassert(upnpc);

    upnpc->to_quit = 0;
    ret = vthread_init(&upnpc->thread, _aux_upnpc_thread_entry, upnpc);
    vlog((ret < 0), elog_vthread_init);
    retE((ret < 0));
    vthread_start(&upnpc->thread);
    return 0;
}

static
int _vupnpc_stop(struct vupnpc* upnpc)
{
    int quit_code = 0;
    vassert(upnpc);

    upnpc->to_quit = 1;
    vcond_signal(&upnpc->cond);
    vthread_join(&upnpc->thread, &quit_code);
    return 0;
}

static
int _vupnpc_set_internal_port(struct vupnpc* upnpc, uint16_t iport)
{
    vassert(upnpc);
    vassert(iport > 0);

    vlock_enter(&upnpc->lock);
    if (upnpc->iport != iport) {
        upnpc->old_iport = upnpc->iport;
        upnpc->iport  = iport;
        vcond_signal(&upnpc->cond);
    }
    vlock_leave(&upnpc->lock);
    return 0;
}

static
int _vupnpc_set_external_port(struct vupnpc* upnpc, uint16_t eport)
{
    vassert(upnpc);
    vassert(eport > 0);

    vlock_enter(&upnpc->lock);
    if (upnpc->eport != eport) {
        upnpc->old_eport = upnpc->eport;
        upnpc->eport = eport;
        vcond_signal(&upnpc->cond);
    }
    vlock_leave(&upnpc->lock);
    return 0;
}

static
int _vupnpc_get_internal_addr(struct vupnpc* upnpc, struct sockaddr_in* iaddr)
{
    int ret = -1;

    vassert(upnpc);
    vassert(iaddr);

    vlock_enter(&upnpc->lock);
    if (UPNPC_ACTIVE == upnpc->state) {
        vsockaddr_copy(iaddr, &upnpc->iaddr);
        ret = 0;
    }
    vlock_leave(&upnpc->lock);
    return ret;
}

static
int _vupnpc_get_external_addr(struct vupnpc* upnpc, struct sockaddr_in* eaddr)
{
    int ret = -1;

    vassert(upnpc);
    vassert(eaddr);

    vlock_enter(&upnpc->lock);
    if (UPNPC_ACTIVE == upnpc->state) {
        vsockaddr_copy(eaddr, &upnpc->eaddr);
        ret = 0;
    }
    vlock_leave(&upnpc->lock);
    return ret;
}

static
int _vupnpc_is_active(struct vupnpc* upnpc)
{
    int ret = 0;
    vassert(upnpc);

    vlock_enter(&upnpc->lock);
    ret = (UPNPC_ACTIVE == upnpc->state);
    vlock_leave(&upnpc->lock);
    return ret;
}

static
struct vupnpc_ops upnpc_ops = {
    .start             = _vupnpc_start,
    .stop              = _vupnpc_stop,
    .set_internal_port = _vupnpc_set_internal_port,
    .set_external_port = _vupnpc_set_external_port,
    .get_internal_addr = _vupnpc_get_internal_addr,
    .get_external_addr = _vupnpc_get_external_addr,
    .is_active         = _vupnpc_is_active
};

int vupnpc_init(struct vupnpc* upnpc)
{
    vassert(upnpc);

    memset(upnpc, 0, sizeof(*upnpc));
    memset(&upnpc_config, 0, sizeof(upnpc_config));
    upnpc->config = &upnpc_config;
    upnpc->state  = UPNPC_RAW;
    upnpc->action = 0;
    upnpc->proto  = UPNP_PROTO_UDP;
    upnpc->iport  = 14999;
    upnpc->eport  = 15999;

    vlock_init(&upnpc->lock);
    vcond_init(&upnpc->cond);

    upnpc->ops     = &upnpc_ops;
    upnpc->act_ops = &upnpc_act_ops;
    return 0;
}

void vupnpc_deinit(struct vupnpc* upnpc)
{
    vassert(upnpc);

    upnpc->ops->stop(upnpc);
    vlock_deinit(&upnpc->lock);
    vcond_deinit(&upnpc->cond);

    return ;
}

