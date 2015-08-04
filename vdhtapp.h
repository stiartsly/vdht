#ifndef __VDHTAPP_H__
#define __VDHTAPP_H__

#include <netinet/in.h>
#include <syslog.h>

#define VSRVCHASH_LEN 20
struct vsrvcHash {
    uint8_t data[VSRVCHASH_LEN];
};
typedef struct vsrvcHash vsrvcHash;

enum {
    VPROTO_RES0,
    VPROTO_UDP,
    VPROTO_TCP,
    VPROTO_RES1,
    VPROTO_RES2,
    VPROTO_UNKNOWN
};

enum {
    VSOCKADDR_LOCAL,
    VSOCKADDR_UPNP,
    VSOCKADDR_REFLEXIVE,
    VSOCKADDR_RELAY,
    VSOCKADDR_UNKNOWN,
    VSOCKADDR_BUTT
};

enum {
    VLOG_ERR    = LOG_ERR,
    VLOG_INFO   = LOG_INFO,
    VLOG_DEBUG  = LOG_DEBUG,
    VLOG_BUTT
};

/*
 * the API to make vdthd daemon to start running. generally, this API is used
 * to start vdthd when vdhtd is offline, which essentially useful for debugging
 * vdhtd.
 *
 */
int  vdhtapp_start_host(void);

/*
 * the API to make vdthd daemon be offline. generally, this API is used
 * to stop vdthd when vdhtd is running, which essentially useful for debugging
 * vdhtd.
 *
 */
int  vdhtapp_stop_host(void);

/*
 * the API to make vdthd daemon exit from running. generally, this API is
 * used to control vdhtd to exit, which essentially useful for debugging
 * vdhtd.
 *
 */
void vdhtapp_make_host_exit (void);

/*
 * the API to make vdthd daemon dump running infos. generally, which
 * essentially be useful for debugging vdhtd.
 *
 */
void vdhtapp_dump_host_infos(void);

/*
 * the API to make vdthd daemon dump config infos. generally, which
 * essentially be useful for debugging vdhtd.
 *
 */
void vdhtapp_dump_cfg (void);

/*
 * the API to ask vdthd daemon to join a wellknown node. generally, which
 * essentially be useful for debugging vdhtd.
 *
 * @addr: socket address to wellknown node
 *
 */
int vdhtapp_join_wellknown_node(struct sockaddr_in* addr);

/*
 * the API to ask vdthd daemon to ping a wellknown node. generally, which
 * essentially be useful for debugging vdhtd.
 *
 * @addr: socket address to wellknown node.
 */
int vdhtapp_bogus_query (int queryId, struct sockaddr_in* addr);


/*
 * the API to ask vdhtd daemon publish a service segment
 * NOTICE: one service only support one protocol;
 *
 * @hash: hash value to service to post;
 * @addr: socket address to service
 * @type: socket address type;
 * @proto: protocol to service, currently supported for udp and tcp.
 *         tcp: VPROTO_TCP,
 *         udp: VPROTO_UDP
 *
 */
int vdhtapp_post_service_segment  (vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int proto);

/*
 * the API to unpost a service segment.
 *
 * @hash: servic hash;
 * @addr: socket address to service
 *
 */
int vdhtapp_unpost_service_segment(vsrvcHash* hash, struct sockaddr_in* addr);

/*
 * the API to post a whole service to vdhtd Daemon.
 *
 * @hash: service hash;
 * @addrs: array of service addresses;
 * @types: array of address types;
 * @sz:   size of array of service addresses.
 * @proto: service protocol, currently supported for udp and tcp
 *        tcp: VPROTO_TCP;
 *        udp: VPROTO_UDP;
 */
int vdhtapp_post_service  (vsrvcHash* hash, struct sockaddr_in* addrs, uint32_t* types, int sz, int proto);

/*
 * the API to unpost a whole service
 *
 * @hash: service hash;
 *
 */
int vdhtapp_unpost_service(vsrvcHash* hash);


/* the callback prototype to inform the address number and protocol used.
 *
 * @hash: service hash
 * @num:  the number of socket addresses. 0 means no services found, and no further
 *        infos need to be care.
 * @proto: protocol of service, currently supported for udp and tcp
 *        tcp: VPROTO_TCP;
 *        udp: VPROTO_UDP;
 * @cookie: the private data from API @vdhtc_find_service or @vdhtc_probe_service
 *
 */
typedef void (*vsrvcInfo_number_addr_t) (vsrvcHash* hash, int num, int proto, void* cookie);

/* the callback prototype to inform one address of service.
 *
 * @hash: service hash
 * @addr: service address
 * @type: sockaddr type;
 * @last: bool value to indcate wheather this invoke is last or not.
 * @cookie: the private data from API @vdhtc_find_service or @vdhtc_probe_service
 *
 */
typedef void (*vsrvcInfo_iterate_addr_t)(vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type,  int last, void* cookie);


/*
 * the API to find a service by hash value.
 * The semantic of this API is to find a service in local vdhtd daemon, which
 * means if the wanted service is owned by local vdhtd daemon or is in service
 * routing space, then the service will be found.
 *   If the service is found, the callback @ncb will be invoked at first to
 * indicate how many addresses owned by service and what protocol that service
 * used. After that, @icb callback will invoked to show all addresses one by one.
 *   If no services are found, the @ncb callback still will be invoked to show
 * no service found with zero value to second argument of callback.
 *
 * NOTIC: callbacks @ncb and icb are invoked before API call returns. so, it's
 * more inclined to be synchronized call.
 *
 * @hash: service hash;
 * @ncb:  callback to show addresses number and protocol to use;
 * @icb:  callback to show address
 * @cookie: private data for user.
 *
 */
int vdhtapp_find_service  (vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie);

/*
 * the API to probe a service by hash value
 * the semantic of this API is to find a service in neighbors of local vdhtd node,
 * which means vdhtd daemon directly broadcast the search request to it's all
 * neighbors nodes, and if one of it's neighbors find this service and response
 * back, then means the service is found.
 *    same as API above, if no service is found (means no neigbor nodes reponse
 * back), then @ncb callback still be invoked, but only be responsible for informing
 * no services found.
 *
 * NOTICE: Being like to API above, callbacks @ncb and @icb are invoked before
 * function return.
 *
 * @hash: service hash;
 * @ncb:  callback to show addresses number and protocol to use;
 * @icb:  callback to show address
 * @cookie: private data for user.
 *
 */
int vdhtapp_probe_service (vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);

/*
 * the API to initialize vdhtapp API module.
 * @cfgfile:
 * @cons_print_level:
 */
int  vdhtapp_init  (const char* cfgfile, int cons_print_level);

/*
 * the API to destroy vdhtapp API module.
 */
void vdhtapp_deinit(void);

/*
 * the API to make vdhtapi API module start to work.
 */
int  vdhtapp_run   (void);

#endif

