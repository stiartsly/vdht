#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "vdhtapi.h"
#include "vhashgen.h"
#include "vlsctlc.h"

#define VSRVC_MAGIC_LEN ((int)128)

__thread int verrno = 0;

#define BUF_SZ ((int)1024)
enum {
    vsucc = 0,
    verr_bad_args,
    verr_outof_mem,

};

enum {
    VDHT_PING,
    VDHT_PING_R,
    VDHT_FIND_NODE,
    VDHT_FIND_NODE_R,
    VDHT_FIND_CLOSEST_NODES,
    VDHT_FIND_CLOSEST_NODES_R,
    VDHT_REFLEX,
    VDHT_REFLEX_R,
    VDHT_PROBE,
    VDHT_PROBE_R,
    VDHT_POST_SERVICE,
    VDHT_FIND_SERVICE,
    VDHT_FIND_SERVICE_R,
    VDHT_UNKNOWN
};

static const char* def_lsctlc_socket = "/var/run/vdht/lsctl_client";
static const char* def_lsctls_socket = "/var/run/vdht/lsctl_socket";
static char glsctlc_socket[BUF_SZ];
static char glsctls_socket[BUF_SZ];


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
int vdhtc_hash(uint8_t* magic, int len, vsrvcHash* hash)
{
    struct vhashgen hashgen;
    int ret = 0;

    if (!magic || len != VSRVC_MAGIC_LEN) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!hash) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vhashgen_init(&hashgen);
    if (ret < 0) {
        verrno = verr_outof_mem;
        return -1;
    }
    ret = hashgen.ops->hash(&hashgen, magic, len, hash);
    vhashgen_deinit(&hashgen);
    if (ret < 0) {
        verrno = verr_bad_args;
        return -1;
    }
    return 0;
}

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
int vdhtc_hash_with_cookie(uint8_t* magic, int len, uint8_t* cookie, int cookie_len, vsrvcHash* hash)
{
    struct vhashgen hashgen;
    int ret = 0;

    if (!magic || len != VSRVC_MAGIC_LEN) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!cookie || cookie_len <= 0) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!hash) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vhashgen_init(&hashgen);
    if (ret < 0) {
        verrno = verr_outof_mem;
        return -1;
    }
    ret = hashgen.ops->hash_with_cookie(&hashgen, magic, len, cookie, cookie_len, hash);
    vhashgen_deinit(&hashgen);
    if (ret < 0) {
        verrno = verr_bad_args;
        return -1;
    }
    return 0;
}

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
int vdhtc_init(const char* client_socket, const char* lsctl_socket)
{
    if (!client_socket && !lsctl_socket) {
        verrno = verr_bad_args;
        return -1;
    }
    if (client_socket && (strlen(client_socket) + 1 >= BUF_SZ)){
        verrno = verr_bad_args;
        return -1;
    }
    if (lsctl_socket && (strlen(lsctl_socket) + 1 >= BUF_SZ)){
        verrno = verr_bad_args;
        return -1;
    }

    memset(glsctlc_socket, 0, BUF_SZ);
    memset(glsctls_socket, 0, BUF_SZ);
    if (client_socket) {
        strcpy(glsctlc_socket, client_socket);
    } else {
        strcpy(glsctlc_socket, def_lsctlc_socket);
    }
    if (lsctl_socket) {
        strcpy(glsctls_socket, lsctl_socket);
    } else {
        strcpy(glsctls_socket, def_lsctls_socket);
    }
    return 0;
}

/*
 * the API to deinitialize vdht API module after finishing work.
 *
 */
void vdhtc_deinit(void)
{
    //do nothing;
    return ;
}

static
int _aux_dhtc_request(void* buf, int len)
{
    struct sockaddr_un caddr;
    struct sockaddr_un saddr;
    int ret = 0;
    int fd = 0;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket:");
        return -1;
    }
    unlink(glsctlc_socket);
    caddr.sun_family = AF_UNIX;
    strcpy(caddr.sun_path, glsctlc_socket);
    ret = bind(fd, (struct sockaddr*)&caddr, sizeof(caddr));
    if (ret < 0) {
        perror("bind:");
        close(fd);
        return -1;
    }
    saddr.sun_family = AF_UNIX;
    strcpy(saddr.sun_path, glsctls_socket);
    ret = sendto(fd, buf, len, 0, (struct sockaddr*)&saddr, sizeof(saddr));
    if (ret < 0) {
        perror("sendto:");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static
int _aux_dhtc_req_and_rsp(void* snd_buf, int snd_len, void* rcv_buf, int rcv_len)
{
    struct sockaddr_un caddr;
    struct sockaddr_un saddr;
    int saddr_len = sizeof(struct sockaddr_un);
    int ret = 0;
    int fd = 0;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket:");
        return -1;
    }
    unlink(glsctlc_socket);
    caddr.sun_family = AF_UNIX;
    strcpy(caddr.sun_path, glsctlc_socket);
    ret = bind(fd, (struct sockaddr*)&caddr, sizeof(caddr));
    if (ret < 0) {
        perror("bind:");
        close(fd);
        return -1;
    }
    saddr.sun_family = AF_UNIX;
    strcpy(saddr.sun_path, glsctls_socket);
    ret = sendto(fd, snd_buf, snd_len, 0, (struct sockaddr*)&saddr, sizeof(saddr));
    if (ret < 0) {
        perror("sendto:");
        close(fd);
        return -1;
    }

    memset(rcv_buf, 0, rcv_len);
    ret = recvfrom(fd, rcv_buf, rcv_len, 0, (struct sockaddr*)&saddr, (socklen_t*)&saddr_len);
    if (ret < 0) {
        perror("recvfrom:");
        close(fd);
        return -1;
    }
    close(fd);
    return ret;
}

/*
 * the API to request vdthd daemon to start running. generally, this API is used
 * to start vdthd when vdhtd is offline, which essentially useful for debugging
 * vdhtd.
 *
 */
int vdhtc_start_host(void)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_host_up(&lsctlc);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to request vdthd daemon to be offline. generally, this API is used
 * to stop vdthd when vdhtd is running, which essentially useful for debugging
 * vdhtd.
 *
 */
int vdhtc_stop_host(void)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_host_down(&lsctlc);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to request vdthd daemon to exit from running. generally, this API is
 * used to command vdhtd to exit, which essentially useful for debugging
 * vdhtd.
 *
 */
int vdhtc_make_host_exit(void)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_host_exit(&lsctlc);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to request vdthd daemon to dump running infos. generally, which
 * essentially be useful for debugging vdhtd.
 *
 */
int vdhtc_dump_host_infos(void)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_host_dump(&lsctlc);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to request vdthd daemon to dump config infos. generally, which
 * essentially be useful for debugging vdhtd.
 *
 */
int vdhtc_dump_cfg_infos(void)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_cfg_dump(&lsctlc);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to request vdthd daemon to join a wellknown node. generally, which
 * essentially be useful for debugging vdhtd.
 *
 * @addr: socket address to service.
 */
int vdhtc_join_wellknown_node(struct sockaddr_in* addr)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    if (!addr) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_join_node(&lsctlc, addr);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to request vdthd daemon to ping a wellknown node. generally, which
 * essentially be useful for debugging vdhtd.
 *
 * @addr: socket address to service.
 */
int vdhtc_request_bogus_ping (struct sockaddr_in* addr)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    if (!addr) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_bogus_query(&lsctlc, VDHT_PING, addr);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;
error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to publish service segment and post it vdhtd daemon.
 * NOTICE: one service only support one protocol;
 *
 * @hash: hash value to service to post;
 * @addr: socket address to service
 * @proto: protocol to service, currently supported for udp and tcp.
 *         tcp: VPROTO_TCP,
 *         udp: VPROTO_UDP
 *
 */
int vdhtc_post_service_segment(vsrvcHash* srvcHash, struct sockaddr_in* addr, uint32_t type, int proto)
{
    struct vsockaddr_in vaddr;
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    if (!srvcHash || !addr) {
        verrno = verr_bad_args;
        return -1;
    }

    if (type >= VSOCKADDR_BUTT) {
        verrno = verr_bad_args;
        return -1;
    }

    if ((proto <= VPROTO_RES0) || (proto > VPROTO_TCP)) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }

    memcpy(&vaddr.addr, addr, sizeof(*addr));
    vaddr.type = type;
    ret = lsctlc.bind_ops->bind_post_service(&lsctlc, srvcHash, &vaddr, 1, proto);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to unpost a service segment.
 *
 * @hash: servic hash;
 * @addr: socket address to service
 *
 */
int vdhtc_unpost_service_segment(vsrvcHash* srvcHash, struct sockaddr_in* addr)
{
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    if (!srvcHash || !addr) {
        verrno = verr_bad_args;
        return -1;
    }
    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_unpost_service(&lsctlc, srvcHash, addr, 1);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to post a whole service to vdhtd Daemon.
 *
 * @hash: service hash;
 * @addr: array of service addresses;
 * @sz:   size of array of service addresses.
 * @proto: service protocol, currently supported for udp and tcp
 *        tcp: VPROTO_TCP;
 *        udp: VPROTO_UDP;
 */
int vdhtc_post_service(vsrvcHash* srvcHash, struct sockaddr_in* addrs, uint32_t* type, int num, int proto)
{
    struct vsockaddr_in vaddrs[VSRVCINFO_MAX_ADDRS];
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;
    int i = 0;

    if (!srvcHash) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!addrs || !type) {
        verrno = verr_bad_args;
        return -1;
    }
    if ((num <= 0) || (num > VSRVCINFO_MAX_ADDRS)) {
        verrno = verr_bad_args;
        return -1;
    }
    if ((proto <= VPROTO_RES0) || (proto > VPROTO_TCP)) {
        verrno = verr_bad_args;
        return -1;
    }
    for (i = 0; i < num; i++) {
        if (type[i] >= VSOCKADDR_BUTT) {
            verrno = verr_bad_args;
            return -1;
        }
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }

    for (i = 0; i < num; i++) {
        memcpy(&vaddrs[i].addr, &addrs[i], sizeof(struct sockaddr_in));
        vaddrs[i].type = type[i];
    }
    ret = lsctlc.bind_ops->bind_post_service(&lsctlc, srvcHash, vaddrs, num, proto);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to unpost a whole service
 *
 * @hash: service hash;
 *
 */
int vdhtc_unpost_service(vsrvcHash* srvcHash)
{
    struct sockaddr_in addr;
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;

    if (!srvcHash) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_unpost_service(&lsctlc, srvcHash, &addr, 0);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    vlsctlc_deinit(&lsctlc);

    ret = _aux_dhtc_request(buf, ret);
    if (ret < 0) {
        return -1;
    }
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

/*
 * the API to find a service by hash value.
 * The semantics of this API is to find a service in local vdhtd daemon, which
 * means if the wanted service is owned by local vdhtd daemon or is in service
 * routing space, then the service will be found.
 *   If the service is found, the callback @ncb will be invoked at first to
 * indicate how many addresses owned by service and what protocol that service
 * used. After that, @icb callback will invoked to show all addresses one by one.
 *   If no services are found, the @ncb callback still will be invoked to show
 * no service found with zero value to second argument of callback.
 *
 * @hash: service hash;
 * @ncb:  callback to show addresses number and protocol to use;
 * @icb:  callback to show address
 * @cookie: private data for user.
 *
 */
int vdhtc_find_service(vsrvcHash* srvcHash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    vsrvcHash hash;
    int proto = 0;
    int num = 0;
    int ret = 0;
    int i = 0;

    if (!srvcHash || !ncb || !icb) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_find_service(&lsctlc, srvcHash);
    if (ret < 0) {
        vlsctlc_deinit(&lsctlc);
        return -1;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    ret = _aux_dhtc_req_and_rsp(buf, ret, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    ret = lsctlc.ops->unpack_cmd(&lsctlc, buf, ret);
    if (ret < 0) {
        goto error_exit;
    }
    ret = lsctlc.unbind_ops->unbind_find_service(&lsctlc, &hash, addrs, &num, &proto);
    if ((ret < 0) || memcmp(&hash, srvcHash, sizeof(vsrvcHash))) {
        goto error_exit;
    }
    ncb(&hash, num, proto, cookie);
    for (i = 0; i < num; i++) {
        icb(&hash, &addrs[i].addr, addrs[i].type, ((i + 1) == num), cookie);
    }
    vlsctlc_deinit(&lsctlc);
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

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
int vdhtc_probe_service(vsrvcHash* srvcHash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    vsrvcHash hash;
    int proto = 0;
    int num = 0;
    int ret = 0;
    int i = 0;

    if (!srvcHash || !ncb || !icb) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }
    ret = lsctlc.bind_ops->bind_probe_service(&lsctlc, srvcHash);
    if (ret < 0) {
        goto error_exit;
    }
    memset(buf, 0, BUF_SZ);
    ret = lsctlc.ops->pack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    ret = _aux_dhtc_req_and_rsp(buf, ret, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }

    ret = lsctlc.ops->unpack_cmd(&lsctlc, buf, BUF_SZ);
    if (ret < 0) {
        goto error_exit;
    }
    ret = lsctlc.unbind_ops->unbind_probe_service(&lsctlc, &hash, addrs, &num, &proto);
    if ((ret < 0) || memcmp(&hash, srvcHash, sizeof(vsrvcHash))) {
        goto error_exit;
    }
    ncb(&hash, num, proto, cookie);
    for (i = 0; i < num; i++) {
        icb(&hash, &addrs[i].addr, addrs[i].type, ((i + 1) == num), cookie);
    }
    vlsctlc_deinit(&lsctlc);
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

