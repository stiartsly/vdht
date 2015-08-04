#include "vglobal.h"

static
struct vappmain* gdhtapp = NULL;

int vdhtapp_start_host(void)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->start_host(gdhtapp);
}

int vdhtapp_stop_host(void)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->stop_host(gdhtapp);
}

void vdhtapp_make_host_exit(void)
{
    if (!gdhtapp) {
        return;
    }
    gdhtapp->api_ops->make_host_exit(gdhtapp);
    return ;
}

void vdhtapp_dump_host_infos(void)
{
    if (!gdhtapp) {
        return;
    }
    gdhtapp->api_ops->dump_host_info(gdhtapp);
    return ;
}

void vdhtapp_dump_cfg (void)
{
    if (!gdhtapp) {
        return;
    }
    gdhtapp->api_ops->dump_cfg_info(gdhtapp);
    return ;
}

int vdhtapp_join_wellknown_node(struct sockaddr_in* addr)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->join_wellknown_node(gdhtapp, addr);
}

int vdhtapp_bogus_query(int queryId, struct sockaddr_in* addr)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->bogus_query(gdhtapp, queryId, addr);
}

int vdhtapp_post_service_segment(vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int proto)
{
    struct vsockaddr_in vaddr;
    if (!gdhtapp) {
        return -1;
    }
    vsockaddr_copy(&vaddr.addr, addr);
    vaddr.type = type;

    return gdhtapp->api_ops->post_service_segment(gdhtapp, hash, &vaddr, proto);
}

int vdhtapp_unpost_service_segment(vsrvcHash* hash, struct sockaddr_in* addr)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->unpost_service_segment(gdhtapp, hash, addr);
}

int vdhtapp_post_service  (vsrvcHash* hash, struct sockaddr_in* addrs, uint32_t* types, int sz, int proto)
{
    struct vsockaddr_in vaddrs[VSRVCINFO_MAX_ADDRS];
    int i = 0;

    if (!gdhtapp) {
        return -1;
    }
    if (sz > VSRVCINFO_MAX_ADDRS) {
        return -1;
    }

    for (i = 0; i < sz; i++) {
        vsockaddr_copy(&vaddrs[i].addr, &addrs[i]);
        vaddrs[i].type = types[i];
    }
    return gdhtapp->api_ops->post_service(gdhtapp, hash, vaddrs, sz, proto);
}

int vdhtapp_unpost_service(vsrvcHash* hash)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->unpost_service(gdhtapp, hash);
}

int vdhtapp_find_service(vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->find_service(gdhtapp, hash, ncb, icb, cookie);
}

int vdhtapp_probe_service(vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->api_ops->probe_service(gdhtapp, hash, ncb, icb, cookie);
}

int vdhtapp_init (const char* cfgfile, int cons_print_level)
{
    if (gdhtapp) {
        return 0;
    }
    gdhtapp = (struct vappmain*)malloc(sizeof(struct vappmain));
    if (!gdhtapp) {
        printf("malloc error\n");
        return -1;
    }
    return vappmain_init(gdhtapp, cfgfile, cons_print_level);
}

void vdhtapp_deinit(void)
{
    if (!gdhtapp) {
        return ;
    }
    vappmain_deinit(gdhtapp);
    gdhtapp = NULL;

    return ;
}

int vdhtapp_run(void)
{
    if (!gdhtapp) {
        return -1;
    }
    return gdhtapp->ops->run(gdhtapp);
}

