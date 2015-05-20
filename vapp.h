#ifndef __VAPP_H__
#define __VAPP_H__

#include "vhost.h"
#include "vcfg.h"
#include "vlsctl.h"

struct vappmain;
struct vappmain_ops {
    int (*run) (struct vappmain*);
};

struct vappmain_api_ops {
    int  (*start_host)            (struct vappmain*);
    int  (*stop_host)             (struct vappmain*);
    void (*make_host_exit)        (struct vappmain*);
    void (*dump_host_info)        (struct vappmain*);
    void (*dump_cfg_info)         (struct vappmain*);
    int  (*join_wellknown_node)   (struct vappmain*, struct sockaddr_in*);
    int  (*bogus_query)           (struct vappmain*, int, struct sockaddr_in*);
    int  (*post_service_segment)  (struct vappmain*, vsrvcHash*, struct vsockaddr_in*, int);
    int  (*post_service)          (struct vappmain*, vsrvcHash*, struct vsockaddr_in*, int, int);
    int  (*unpost_service_segment)(struct vappmain*, vsrvcHash*, struct sockaddr_in*);
    int  (*unpost_service)        (struct vappmain*, vsrvcHash*);
    int  (*find_service)          (struct vappmain*, vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);
    int  (*probe_service)         (struct vappmain*, vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);
    const char* (*get_version)    (struct vappmain*);
};

struct vappmain {
    int  need_stdout;
    int  daemonized;

    struct vhost   host;
    struct vconfig cfg;
    struct vlsctl  lsctl;
    struct vappmain_ops* ops;
    struct vappmain_api_ops* api_ops;
};

int  vappmain_init  (struct vappmain*, const char*, int);
void vappmain_deinit(struct vappmain*);

#endif

