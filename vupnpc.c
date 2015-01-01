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
const char* upnp_proto_names[] = {
    "udp",
    "tcp",
    0
};

static
int _vupnpc_setup(struct vupnpc* upnpc)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    int ret = 0;
    vassert(upnpc);

    memset(cfg, 0, sizeof(*cfg));
    cfg->devlist = upnpDiscover(2000, NULL, NULL, 0, 0, NULL);
    if (!cfg->devlist) {
        vlogE((printf("No IGD found")));
        upnpc->state = UPNPC_NOIGD;
        return -1;
    }

    ret = UPNP_GetValidIGD(cfg->devlist, &cfg->urls, &cfg->data, cfg->lan_addr, 32);
    if (!ret) {
        vlogE(printf("No IGD found"));
        freeUPNPDevlist(cfg->devlist);
        upnpc->state = UPNPC_NOIGD;
        return -1;
    }

    upnpc->state = UPNPC_READY;
    return 0;
}

static
int _vupnpc_shutdown(struct vupnpc* upnpc)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    vassert(upnpc);

    switch(upnpc->state) {
    case UPNPC_ACTIVE:
        //todo;
        break;
    case UPNPC_READY:
        freeUPNPDevlist(cfg->devlist);
        upnpc->state = UPNPC_RAW;
        break;

    case UPNPC_RAW:
    case UPNPC_NOIGD:
    default:
        upnpc->state = UPNPC_RAW;
        break;
    }
    return 0;
}

static
int _vupnpc_map_port(struct vupnpc* upnpc, int in_port, int ext_port, int proto, struct sockaddr_in* ext_addr)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    char in_addr_str[32];
    char in_port_str[8];
    char ext_addr_str[32];
    char ext_port_str[8];
    char proto_str[8];
    char in_addr_tmp[32];
    char in_port_tmp[8];
    char desc_str[32];
    char duration_str[32];
    int ret = 0;

    vassert(upnpc);
    vassert(in_port  > 0);
    vassert(ext_port > 0);
    vassert(proto >= 0);
    vassert(proto < UPNPC_PROTO_BUTT);
    vassert(ext_addr);

    memset(in_addr_str,  0, sizeof(in_addr_str));
    memset(in_port_str,  0, sizeof(in_port_str));
    memset(ext_addr_str, 0, sizeof(ext_addr_str));
    memset(ext_port_str, 0, sizeof(ext_port_str));
    strcpy(proto_str, upnp_proto_names[proto]);

    //todo;
    ret = UPNP_GetExternalIPAddress(cfg->urls.controlURL, cfg->data.CIF.servicetype, ext_addr_str);
    if ((ret < 0) || (!ext_addr_str[0])) {
        vlogE(printf("UPNP_GetExternalIPAddress"));
        vlogI(printf("Failed to get external ip"));
        return -1;
    }

    ret = UPNP_AddPortMapping(cfg->urls.controlURL,
                cfg->data.CIF.servicetype,
                ext_port_str,
                in_port_str,
                in_addr_str,
                desc_str,
                proto_str,
                NULL,
                duration_str);
    vlog((!ret), vlogE(printf("UPNP_AddPortMapping")));
    retE((!ret));

    ret = UPNP_GetSpecificPortMappingEntry(cfg->urls.controlURL,
                cfg->data.CIF.servicetype,
                ext_port_str,
                proto_str,
                NULL,
                in_addr_tmp,
                in_port_tmp,
                NULL,
                NULL,
                NULL);
    if ((ret < 0) || (!in_addr_tmp[0])) {
        vlogE(printf("UPNP_GetSpecifiicPortMappingEntry"));
        return -1;
    }

    if (!strcmp(in_addr_str, in_addr_tmp) && !strcmp(in_port_str, in_port_tmp)) {
        vlogI(printf("PortMappingEntry to wrong location"));
        return -1;
    }

    ret = vsockaddr_convert(ext_addr_str, ext_port, ext_addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));
    vlogI(printf("uPnP Forward/Mapping Succeeded"));
    return 0;
}

static
int _vupnpc_unmap_port(struct vupnpc* upnpc, int ext_port, int proto)
{
    struct vupnpc_config* cfg = (struct vupnpc_config*)upnpc->config;
    char ext_port_str[8];
    char proto_str[8];
    int ret = 0;

    vassert(upnpc);
    vassert(ext_port > 0);
    vassert(proto >= 0);
    vassert(proto < UPNPC_PROTO_BUTT);

    memset(ext_port_str, 0, sizeof(ext_port_str));
    strcpy(proto_str, upnp_proto_names[proto]);

    memset(ext_port_str, 0, sizeof(ext_port_str));
    ret = UPNP_DeletePortMapping(cfg->urls.controlURL, cfg->data.CIF.servicetype, ext_port_str, proto_str, NULL);
    vlog((!ret), vlogE(printf("UPNP_DeletePortmapping")));
    retE((!ret));

    upnpc->state = UPNPC_READY;
    return 0;
}

static
int _vupnpc_get_status(struct vupnpc* upnpc, struct vupnpc_status* status)
{
    vassert(upnpc);
    vassert(status);

    //todo;
    return 0;
}

static
void _vupnpc_dump_mapping_port(struct vupnpc* upnpc)
{
    vassert(upnpc);

    //todo;
    return ;
}

static
struct vupnpc_ops upnpc_ops = {
    .setup             = _vupnpc_setup,
    .shutdown          = _vupnpc_shutdown,
    .map_port          = _vupnpc_map_port,
    .unmap_port        = _vupnpc_unmap_port,
    .get_status        = _vupnpc_get_status,
    .dump_mapping_port = _vupnpc_dump_mapping_port
};

int vupnpc_init(struct vupnpc* upnpc)
{
    vassert(upnpc);

    memset(&upnpc_config, 0, sizeof(upnpc_config));
    upnpc->config = &upnpc_config;
    upnpc->state  = UPNPC_RAW;
    vlock_init(&upnpc->lock);

    upnpc->ops = &upnpc_ops;
    return 0;
}

void vupnpc_deinit(struct vupnpc* upnpc)
{
    vassert(upnpc);

    upnpc->ops->shutdown(upnpc);

    //todo;
    return ;
}

