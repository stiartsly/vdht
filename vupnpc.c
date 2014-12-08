#include "vglobal.h"
#include "vupnpc.h"

static
int _vupnpc_map_port(
        struct vupnpc* upnpc,
        int in_port,
        int ext_port,
        int proto,
        int duration,
        char* client)
{
    vassert(upnpc);
    vassert(in_port  > 0);
    vassert(ext_port > 0);
    vassert(proto >= 0);
    vassert(proto < VUPNPC_PROTO_BUTT);
    vassert(client);

    //todo;
    return 0;
}

static
int _vupnpc_unmap_port(struct vupnpc* upnpc, int ext_port, int proto)
{
    vassert(upnpc);
    vassert(ext_port > 0);
    vassert(proto >= 0);
    vassert(proto < VUPNPC_PROTO_BUTT);

    //todo;
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
    .map_port          = _vupnpc_map_port,
    .unmap_port        = _vupnpc_unmap_port,
    .get_status        = _vupnpc_get_status,
    .dump_mapping_port = _vupnpc_dump_mapping_port
};

int vupnpc_init(struct vupnpc* upnpc)
{
    vassert(upnpc);

    upnpc->ops = &upnpc_ops;

    //todo;
    return 0;
}

void vupnpc_deinit(struct vupnpc* upnpc)
{
    vassert(upnpc);
    //todo;

    return ;
}

