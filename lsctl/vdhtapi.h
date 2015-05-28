#ifndef __VDHTC_API_H__
#define __VDHTC_API_H__

#include <netinet/in.h>
#include "vhashgen.h"
#include "vdhtapi_inc.h"

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

/*
 * the API to initialize vdhtc API module. As we know, vdhtapi module use domain
 * socket to communicate with vdhtd daemon.
 *
 * @[in] client_socket: the domain socket of vdhtapi side, by default with
 *                      @"/var/run/vdht/lsctl_client"
 * @[in] lsctl_socket: the domain socket of vdhtd daemon side, by default
 *                      with "/var/run/vdht/lsctl_socket"
 *
 */
int  vdhtc_init(const char* client_socket, const char* lsctl_socket);


/*
 * the API to deinitialize vdht API module after finishing work.
 *
 */
void vdhtc_deinit(void);

/*
 * the API to generate service hash with certian magic, and magic value must
 * comply with the format rule of service magic, which is:
 *      @service-name@service-auth@service-vendor@xxxxxx
 * the total length of magic must be 128 bytes.
 * @vhashgen is tool program to generate service magic, see README.
 *
 * Normally, this API is used to generate the magic of system service, which
 * always has mutliple sources in practice. For example, stun or relay service
 * can deployed many vdht nodes, and service demander (normally client app)
 * can use any one of them, and uncare of service source.
 *
 * @[in] magic: service magic, complied with format rule of servic magic;
 * @[in] len:   length of service magic, must to be 128.
 * @[out]hash:
 *
 */
int vdhtc_hash(uint8_t* magic, int len, vsrvcHash* hash);

/*
 * the API to generate service magic with certian magic and own cookie information,
 * where magic value must comply with format rule of service magic, and cookie
 * infos is normally to be Serial Number of device that uniquely distinguish from
 * others. same as @vdhtc_hash, the length of magic value must be 128 bytes.
 *
 * In practice, this API shoud be used to generate the magic of application service
 * such as Cloud Storage Service. Service like this is characterized as single-
 * source service, which means it docked on unique device or only serve for certain
 * client.
 *
 * @[in] magic: serivce magic, complied with certian format rule.
 * @[in] len  : length of magic value, must to be 128;
 * @[in] cookie: unique identity information to service
 * @[in] cookie_len: length of cookie information;
 * @[out] hash:
 *
 */
int vdhtc_hash_with_cookie(uint8_t* magic, int len, uint8_t* cookie, int cookie_len, vsrvcHash* hash);

/*
 * the API to request vdthd daemon to start running. generally, this API is used
 * to start vdthd when vdhtd is offline, which essentially useful for debugging
 * vdhtd.
 *
 */
int vdhtc_start_host     (void);

/*
 * the API to request vdthd daemon to be offline. generally, this API is used
 * to stop vdthd when vdhtd is running, which essentially useful for debugging
 * vdhtd.
 *
 */
int vdhtc_stop_host      (void);

/*
 * the API to request vdthd daemon to exit from running. generally, this API is
 * used to command vdhtd to exit, which essentially useful for debugging
 * vdhtd.
 *
 */
int vdhtc_make_host_exit (void);

/*
 * the API to request vdthd daemon to dump running infos. generally, which
 * essentially be useful for debugging vdhtd.
 *
 */
int vdhtc_dump_host_infos(void);

/*
 * the API to request vdthd daemon to dump config infos. generally, which
 * essentially be useful for debugging vdhtd.
 *
 */
int vdhtc_dump_cfg_infos (void);

/*
 * the API to request vdthd daemon to join a wellknown node. generally, which
 * essentially be useful for debugging vdhtd.
 *
 * @addr: socket address to service
 *
 */
int vdhtc_join_wellknown_node(struct sockaddr_in* addr);

/*
 * the API to request vdthd daemon to ping a wellknown node. generally, which
 * essentially be useful for debugging vdhtd.
 *
 * @addr: socket address to service.
 */
int vdhtc_request_bogus_ping (struct sockaddr_in* addr);


/*
 * the API to publish service segment and post it vdhtd daemon.
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
int vdhtc_post_service_segment  (vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int proto);

/*
 * the API to unpost a service segment.
 *
 * @hash: servic hash;
 * @addr: socket address to service
 *
 */
int vdhtc_unpost_service_segment(vsrvcHash* hash, struct sockaddr_in* addr);

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
int vdhtc_post_service  (vsrvcHash* hash, struct sockaddr_in* addrs, uint32_t* types, int sz, int proto);

/*
 * the API to unpost a whole service
 *
 * @hash: service hash;
 *
 */
int vdhtc_unpost_service(vsrvcHash* hash);


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
int vdhtc_find_service  (vsrvcHash* hash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie);

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
 * NOTICE: unlike to API above, callbacks @ncb and @icb are invoked after API call
 * returned.
 *
 * @hash: service hash;
 * @ncb:  callback to show addresses number and protocol to use;
 * @icb:  callback to show address
 * @cookie: private data for user.
 *
 */
int vdhtc_probe_service (vsrvcHash*, vsrvcInfo_number_addr_t, vsrvcInfo_iterate_addr_t, void*);

#endif

