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
#include "vnodeId.h"
#include "vlsctlc.h"

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

int vdhtc_hash(uint8_t* msg, int len, vsrvcHash* hash)
{
    struct vhashgen hashgen;
    int ret = 0;

    if (!msg || len <= 0) {
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
    ret = hashgen.ops->hash(&hashgen, msg, len, hash);
    vhashgen_deinit(&hashgen);
    if (ret < 0) {
        verrno = verr_bad_args;
        return -1;
    }
    return 0;
}

int vdhtc_hash_with_cookie(uint8_t* msg, int len, uint8_t* cookie, int cookie_len, vsrvcHash* hash)
{
    struct vhashgen hashgen;
    int ret = 0;

    if (!msg || len <= 0) {
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
    ret = hashgen.ops->hash_with_cookie(&hashgen, msg, len, cookie, cookie_len, hash);
    vhashgen_deinit(&hashgen);
    if (ret < 0) {
        verrno = verr_bad_args;
        return -1;
    }
    return 0;
}

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

int vdhtc_post_service_segment(vsrvcHash* srvcHash, struct sockaddr_in* addr)
{
    struct sockaddr_in* addrs[] = { addr };
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
    ret = lsctlc.bind_ops->bind_post_service(&lsctlc, srvcHash, addrs, 1);
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

int vdhtc_unpost_service_segment(vsrvcHash* srvcHash, struct sockaddr_in* addr)
{
    struct sockaddr_in* addrs[] = { addr };
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
    ret = lsctlc.bind_ops->bind_unpost_service(&lsctlc, srvcHash, addrs, 1);
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

int vdhtc_post_service(vsrvcHash* srvcHash, struct sockaddr_in* addrs, int num)
{
    struct sockaddr_in* addrs_a[12] = {0};
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    int ret = 0;
    int i = 0;

    if (!srvcHash) {
        verrno = verr_bad_args;
        return -1;
    }
    if (!addrs || num <= 0) {
        verrno = verr_bad_args;
        return -1;
    }

    ret = vlsctlc_init(&lsctlc);
    if (ret < 0) {
        return -1;
    }

    for (i = 0; i < num; i++) {
        addrs_a[i] = &addrs[i];
    }
    ret = lsctlc.bind_ops->bind_unpost_service(&lsctlc, srvcHash, addrs_a, num);
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

int vdhtc_unpost_service(vsrvcHash* srvcHash)
{
    struct sockaddr_in* addrs[] = {0};
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
    ret = lsctlc.bind_ops->bind_post_service(&lsctlc, srvcHash, addrs, 0);
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

int vdhtc_find_service(vsrvcHash* srvcHash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    vsrvcHash hash;
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
    ret = lsctlc.unbind_ops->unbind_find_service(&lsctlc, &hash, addrs, &num);
   // if ((ret < 0) || memcmp(&hash, srvcHash, sizeof(vsrvcHash))) {
   //     goto error_exit;
   // }
    ncb(&hash, num, cookie);
    for (i = 0; i < num; i++) {
        icb(&hash, &addrs[i], ((i + 1) == num), cookie);
    }
    vlsctlc_deinit(&lsctlc);
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

int vdhtc_probe_service(vsrvcHash* srvcHash, vsrvcInfo_number_addr_t ncb, vsrvcInfo_iterate_addr_t icb, void* cookie)
{
    struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    struct vlsctlc lsctlc;
    char buf[BUF_SZ];
    vsrvcHash hash;
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
    ret = lsctlc.unbind_ops->unbind_probe_service(&lsctlc, &hash, addrs, &num);
    if ((ret < 0) || memcmp(&hash, srvcHash, sizeof(vsrvcHash))) {
        goto error_exit;
    }
    ncb(&hash, num, cookie);
    for (i = 0; i < num; i++) {
        icb(&hash, &addrs[i], ((i + 1) == num), cookie);
    }
    vlsctlc_deinit(&lsctlc);
    return 0;

error_exit:
    vlsctlc_deinit(&lsctlc);
    return -1;
}

