#ifndef __VDHTC_API_H__
#define __VDHTC_API_H__

#include "vnodeId.h"

int  vdhtc_init  (const char*, const char*);
void vdhtc_deinit(void);

int vdhtc_hash(uint8_t*, int, vsrvcHash*);
int vdhtc_hash_with_cookie(uint8_t*, int, uint8_t*, int, vsrvcHash*);

int vdhtc_post_service_segment  (vsrvcHash*, struct sockaddr_in*);
int vdhtc_unpost_service_segment(vsrvcHash*, struct sockaddr_in*);

int vdhtc_post_service  (vsrvcHash*, struct sockaddr_in*, int);
int vdhtc_unpost_service(vsrvcHash*);

int vdhtc_find_service  (vsrvcHash*, vsrvcInfo_iterate_addr_t, void*);
int vdhtc_probe_service (vsrvcHash*, vsrvcInfo_iterate_addr_t, void*);

#endif

