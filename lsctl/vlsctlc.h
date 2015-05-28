#ifndef __VLSCTLC_H__
#define __VLSCTLC_H__

#include "vdhtapi.h"
#include "vdhtapi_inc.h"

/*
 * lsctl message structure:
 *
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |0 0 0 0|  ver  | message type  |            message length       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                  magic cookie                                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          command Id           |        command length           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          command content   ....                                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#define VLSCTL_MAGIC    ((uint32_t)0x10423373)
#define VLSCTL_VERSION  ((uint8_t)0x1)

enum {
    vlsctl_cmd           = (uint8_t)0x0,
    vlsctl_req           = (uint8_t)0x1,
    vlsctl_rsp_succ      = (uint8_t)0x2,
    vlsctl_rsp_err       = (uint8_t)0x3,
    vlsctl_raw           = (uint8_t)0x4
};

enum {
    VLSCTL_RESERVE       = 0,
    VLSCTL_HOST_UP       = (uint16_t)0x10,
    VLSCTL_HOST_DOWN     = (uint16_t)0x11,
    VLSCTL_HOST_EXIT     = (uint16_t)0x12,
    VLSCTL_HOST_DUMP     = (uint16_t)0x13,
    VLSCTL_CFG_DUMP      = (uint16_t)0x20,

    VLSCTL_BOGUS_QUERY   = (uint16_t)0x25,
    VLSCTL_JOIN_NODE     = (uint16_t)0x26,

    VLSCTL_POST_SERVICE  = (uint16_t)0x30,
    VLSCTL_UNPOST_SERVICE= (uint16_t)0x31,
    VLSCTL_FIND_SERVICE  = (uint16_t)0x32,
    VLSCTL_PROBE_SERVICE = (uint16_t)0x33,

    VLSCTL_BUTT          = (uint16_t)0x99
};

#define VSRVCINFO_MAX_ADDRS ((int16_t)12)
#define VSRVCINFO_MIN_ADDRS ((int16_t)2)

struct vsockaddr_in {
    struct sockaddr_in addr;
    uint32_t type;
};

struct vlsctlc;
typedef int (*vlsctlc_pack_cmd_t)(struct vlsctlc*, void*, int);
struct vlsctlc_pack_cmd_desc {
    const char* desc;
    int32_t cmd_id;
    vlsctlc_pack_cmd_t cmd;
};

typedef int (*vlsctlc_unpack_cmd_t)(struct vlsctlc*, void*, int);
struct vlsctlc_unpack_cmd_desc {
    const char* desc;
    int32_t cmd_id;
    vlsctlc_unpack_cmd_t cmd;
};

struct vlsctlc_bind_ops {
    int (*bind_host_up)    (struct vlsctlc*);
    int (*bind_host_down)  (struct vlsctlc*);
    int (*bind_host_exit)  (struct vlsctlc*);
    int (*bind_host_dump)  (struct vlsctlc*);
    int (*bind_cfg_dump)   (struct vlsctlc*);
    int (*bind_join_node)  (struct vlsctlc*, struct sockaddr_in*);
    int (*bind_bogus_query)(struct vlsctlc*, int, struct sockaddr_in*);
    int (*bind_post_service)   (struct vlsctlc*, vsrvcHash*, struct vsockaddr_in*, int, int);
    int (*bind_unpost_service) (struct vlsctlc*, vsrvcHash*, struct sockaddr_in*, int);
    int (*bind_find_service)   (struct vlsctlc*, vsrvcHash*);
    int (*bind_probe_service)  (struct vlsctlc*, vsrvcHash*);
};

struct vlsctlc_unbind_ops {
    int (*unbind_probe_service)(struct vlsctlc*, vsrvcHash*, struct vsockaddr_in*, int*, int*);
    int (*unbind_find_service) (struct vlsctlc*, vsrvcHash*, struct vsockaddr_in*, int*, int*);
};

struct vlsctlc_ops {
    int (*pack_cmd)        (struct vlsctlc*, void*, int);
    int (*unpack_cmd)      (struct vlsctlc*, void*, int);
};

union vlsctlc_args {
    struct join_node_args {
        struct sockaddr_in addr;
    } join_node_args;

    struct bogus_query_args {
        int32_t queryId;
        struct sockaddr_in addr;
    } bogus_query_args;

    struct post_service_args {
        vsrvcHash hash;
        int32_t proto;
        int32_t naddrs;
        struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } post_service_args;

    struct unpost_service_args {
        vsrvcHash hash;
        int32_t naddrs;
        struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } unpost_service_args;

    struct find_service_args {
        vsrvcHash hash;
    } find_service_args;

    struct probe_service_args {
        vsrvcHash hash;
    } probe_service_args;
};

union vlsctlc_rsp_args {
    struct find_service_rsp_args {
        int32_t num;
        vsrvcHash hash;
        int32_t proto;
        struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } find_service_rsp_args;

    struct probe_service_rsp_args {
        int32_t num;
        vsrvcHash hash;
        int32_t proto;
        struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } probe_service_rsp_args;

    struct error_args {
        int32_t err_val;
    } error_args;
};

struct vlsctlc {
    uint16_t type;
    uint16_t bound_cmd;
    union vlsctlc_args args;
    union vlsctlc_rsp_args rsp_args;

    struct vlsctlc_ops *ops;
    struct vlsctlc_bind_ops* bind_ops;
    struct vlsctlc_unbind_ops* unbind_ops;
};

int  vlsctlc_init  (struct vlsctlc*);
void vlsctlc_deinit(struct vlsctlc*);

#endif

