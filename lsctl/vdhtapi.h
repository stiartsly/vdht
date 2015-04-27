#ifndef __VDHTC_API_H__
#define __VDHTC_API_H__

#include "vnodeId.h"
#include "vhashgen.h"

int  vdhtc_init  (const char*, const char*);
void vdhtc_deinit(void);

int vdhtc_hash(uint8_t*, int, vsrvcHash*);
int vdhtc_hash_with_cookie(uint8_t*, int, uint8_t*, int, vsrvcHash*);

int vdhtc_start_host     (void);
int vdhtc_stop_host      (void);
int vdhtc_make_host_exit (void);
int vdhtc_dump_host_infos(void);
int vdhtc_dump_cfg_infos (void);

int vdhtc_join_wellknown_node(struct sockaddr_in*);
int vdhtc_request_bogus_ping (struct sockaddr_in*);

int vdhtc_post_service_segment  (vsrvcHash*, struct sockaddr_in*);
int vdhtc_unpost_service_segment(vsrvcHash*, struct sockaddr_in*);

int vdhtc_post_service  (vsrvcHash*, struct sockaddr_in*, int);
int vdhtc_unpost_service(vsrvcHash*);

int vdhtc_find_service  (vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);
int vdhtc_probe_service (vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);

#endif

