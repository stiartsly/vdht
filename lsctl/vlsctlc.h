#ifndef __VLSCTLC_H__
#define __VLSCTLC_H__

#include "vnodeId.h"

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
 * |          command Id           |        command length           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          command content   ....                                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | ....                                                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

#define VLSCTL_MAGIC    ((uint32_t)0x10423373)
#define VLSCTL_VERSION  ((uint8_t)0b01)

enum {
    vlsctl_cmd           = (uint8_t)0b00,
    vlsctl_req           = (uint8_t)0b01,
    vlsctl_rsp_succ      = (uint8_t)0b10,
    vlsctl_rsp_err       = (uint8_t)0b11,
    vlsctl_raw           = (uint8_t)0b100,
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
    int (*bind_post_service)   (struct vlsctlc*, vsrvcHash*, struct sockaddr_in**, int);
    int (*bind_unpost_service) (struct vlsctlc*, vsrvcHash*, struct sockaddr_in**, int);
    int (*bind_find_service)   (struct vlsctlc*, vsrvcHash*);
    int (*bind_probe_service)  (struct vlsctlc*, vsrvcHash*);
};

struct vlsctlc_unbind_ops {
    int (*unbind_probe_service)(struct vlsctlc*, vsrvcHash*, struct sockaddr_in*, int*);
    int (*unbind_find_service) (struct vlsctlc*, vsrvcHash*, struct sockaddr_in*, int*);
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
        int queryId;
        struct sockaddr_in addr;
    } bogus_query_args;

    struct post_service_args {
        vsrvcHash hash;
        int naddrs;
        struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } post_service_args;

    struct unpost_service_args {
        vsrvcHash hash;
        int naddrs;
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
        int num;
        vsrvcHash hash;
        struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } find_service_rsp_args;

    struct probe_service_rsp_args {
        int num;
        vsrvcHash hash;
        struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
    } probe_service_rsp_args;

    struct error_args {
        int err_val;
    } error_args;
};

struct vlsctlc {
    int type;
    int bound_cmd;
    union vlsctlc_args args;
    union vlsctlc_rsp_args rsp_args;

    struct vlsctlc_ops *ops;
    struct vlsctlc_bind_ops* bind_ops;
    struct vlsctlc_unbind_ops* unbind_ops;
};

int  vlsctlc_init  (struct vlsctlc*);
void vlsctlc_deinit(struct vlsctlc*);

#endif

