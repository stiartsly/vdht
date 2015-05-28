#include "vglobal.h"
#include "vupnpc.h"
#include "miniupnpc.h"
#include "upnpcommands.h"

struct vupnpc_config {
    struct UPNPDev* devlist;
    struct UPNPUrls urls;
    struct IGDdatas data;
};
static struct vupnpc_config upnpc_cfg;

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

    retE((UPNPC_RAW != upnpc->state));

    memset(cfg, 0, sizeof(*cfg));
    cfg->devlist = upnpDiscover(2000, NULL, NULL, 0, 0, NULL);
    vlogIv((!cfg->devlist), "No IGD found for unpnp mapping");
    retS((!cfg->devlist));

    _aux_dump_IGD_devices(cfg->devlist);

    memset(upnpc->lan_iaddr, 0, 32);
    ret = UPNP_GetValidIGD(cfg->devlist, &cfg->urls, &cfg->data, upnpc->lan_iaddr, 32);
    vlogIv((!ret), "no valid IGD found for upnp mapping");
    if (!ret) {
        freeUPNPDevlist(cfg->devlist);
        return 0;
    }

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
        // fall through next case.
    case UPNPC_READY:
        freeUPNPDevlist(cfg->devlist);
        upnpc->state = UPNPC_RAW;
        break;

    case UPNPC_RAW:
        break;
    default:
        vassert(0);
        break;
    }
    return 0;
}

/* the routine test whether the upnp mapper can work (or mapp port);
 *
 * @upnpc
 */
static
int _vupnpc_workable(struct vupnpc* upnpc)
{
    int workable = 0;
    vassert(upnpc);

    switch(upnpc->state) {
    case UPNPC_ACTIVE:
    case UPNPC_READY:
        workable = 1;
        break;
    case UPNPC_RAW:
    default:
        workable = 0;
        break;
    }
    return workable;
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
int _vupnpc_map(struct vupnpc* upnpc, struct sockaddr_in* laddr, int proto, struct sockaddr_in* eaddr)
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
    int16_t eport = 0;
    char tmp[64];
    int  ret = 0;

    vassert(upnpc);
    retE((UPNPC_READY != upnpc->state) && (UPNPC_ACTIVE != upnpc->state));

    memset(siaddr, 0, 32);
    memset(seaddr, 0, 32);
    memset(siport, 0, 8);
    memset(seport, 0, 8);
    memset(sproto, 0, 8);
    memset(sduration,  0, 8);
    memset(siaddr_tmp, 0, 32);
    memset(siport_tmp, 0, 8);

    {
        uint32_t uiaddr;

        uiaddr = ntohl(laddr->sin_addr.s_addr);
        snprintf(siaddr, 32, "%d.%d.%d.%d",
                (uiaddr >> 24) & 0xff,
                (uiaddr >> 16) & 0xff,
                (uiaddr >> 8 ) & 0xff,
                (uiaddr >> 0 ) & 0xff);
    }
    snprintf(siport, 8, "%d", ntohs(laddr->sin_port));
    snprintf(seport, 8, "%d", ntohs(laddr->sin_port));
    snprintf(sproto, 8, "%s", upnp_protos[proto]);
    eport = ntohs(laddr->sin_port);


    ret = UPNP_GetExternalIPAddress(cfg->urls.controlURL, cfg->data.first.servicetype, seaddr);
    vlogEv((ret < 0), elog_UPNP_GetExternalIPAddress);
    retE((ret < 0));
    vlogEv((!seaddr[0]), "external address is empty");
    retE((!seaddr[0]));

    vlogI("External address: %s", seaddr);

    memset(tmp, 0, 64);
    ret = UPNP_AddPortMapping(cfg->urls.controlURL,
                cfg->data.first.servicetype,
                seport, siport,
                siaddr, "aaaa",
                sproto, 0,
                "0");
    vlogEv((ret < 0), elog_UPNP_AddPortMapping);
    retE((ret < 0));

    ret = UPNP_GetSpecificPortMappingEntry(cfg->urls.controlURL,
                cfg->data.first.servicetype,
                seport, sproto, NULL,
                siaddr_tmp, siport_tmp,
                NULL, NULL, tmp);
    vlogEv((ret < 0), elog_UPNP_GetSpecificPortMappingEntry);
    retE((ret < 0));
    vlogEv((!siaddr_tmp[0]), "local address is empty");
    retE((!siaddr_tmp[0]));

    if (strcmp(siaddr, siaddr_tmp) || strcmp(siport, siport_tmp)) {
        vlogE("Local address is not same");
        return -1;
    }
    vlogI("Local addr:%s:%s", siaddr, siport);
    vsockaddr_convert(seaddr, eport, eaddr);
    upnpc->state = UPNPC_ACTIVE;
    vlogI("uPnP Forward/Mapping Succeeded");
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
int _vupnpc_unmap(struct vupnpc* upnpc, uint16_t eport, int proto)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    char seport[8];
    char sproto[8];
    int ret = 0;

    vassert(upnpc);
    vassert(eport > 0);
    vassert(proto >= 0);
    vassert(proto < UPNP_PROTO_BUTT);
    retE((UPNPC_ACTIVE != upnpc->state));

    memset(seport, 0, 8);
    memset(sproto, 0, 8);

    snprintf(seport, 8, "%d", eport);
    snprintf(sproto, 8, "%s", upnp_protos[proto]);

    ret = UPNP_DeletePortMapping(cfg->urls.controlURL,
                cfg->data.first.servicetype,
                seport, sproto, NULL);
    vlogEv((ret < 0), elog_UPNP_DeletePortMapping);
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
int _vupnpc_status(struct vupnpc* upnpc, struct vupnpc_status* status)
{
    vassert(upnpc);
    vassert(status);

    //todo;
    return 0;
}

struct vupnpc_ops upnpc_ops = {
    .setup    = _vupnpc_setup,
    .shutdown = _vupnpc_shutdown,
    .workable = _vupnpc_workable,
    .map      = _vupnpc_map,
    .unmap    = _vupnpc_unmap,
    .status   = _vupnpc_status
};

int vupnpc_init(struct vupnpc* upnpc)
{
    vassert(upnpc);

    upnpc->config = &upnpc_cfg;
    upnpc->state  = UPNPC_RAW;
    upnpc->ops    = &upnpc_ops;

    return 0;
}

void vupnpc_deinit(struct vupnpc* upnpc)
{
    vassert(upnpc);

    upnpc->ops->shutdown(upnpc);
    return ;
}

