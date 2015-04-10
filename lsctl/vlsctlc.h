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
    VLSCTL_HOST_UP       = (uint16_t)0x10,
    VLSCTL_HOST_DOWN     = (uint16_t)0x11,
    VLSCTL_HOST_EXIT     = (uint16_t)0x12,
    VLSCTL_HOST_DUMP     = (uint16_t)0x13,
    VLSCTL_CFG_DUMP      = (uint16_t)0x20,

    VLSCTL_BOGUS_QUERY   = (uint16_t)0x25,
    VLSCTL_JOIN_NODE     = (uint16_t)0x26,
    VLSCTL_POST_SERVICE  = (uint16_t)0x30,
    VLSCTL_UNPOST_SERVICE= (uint16_t)0x31,
    VLSCTL_PROBE_SERVICE = (uint16_t)0x32,

    VLSCTL_BUTT          = (uint16_t)0x99
};

struct vlsctlc;
typedef int (*vlsctlc_pack_cmd_t)(struct vlsctlc*, void*, int);
struct vlsctlc_pack_cmd_desc {
    const char* desc;
    int32_t cmd_id;
    vlsctlc_pack_cmd_t cmd;
};

struct vlsctlc_bind_ops {
    int (*bind_host_up)    (struct vlsctlc*);
    int (*bind_host_down)  (struct vlsctlc*);
    int (*bind_host_exit)  (struct vlsctlc*);
    int (*bind_host_dump)  (struct vlsctlc*);
    int (*bind_cfg_dump)   (struct vlsctlc*);
    int (*bind_join_node)  (struct vlsctlc*, struct sockaddr_in*);
    int (*bind_bogus_query)(struct vlsctlc*, int, struct sockaddr_in*);
    int (*bind_post_service)  (struct vlsctlc*, vsrvcHash*, struct sockaddr_in*);
    int (*bind_unpost_service)(struct vlsctlc*, vsrvcHash*, struct sockaddr_in*);
    int (*bind_probe_service) (struct vlsctlc*, vsrvcHash*);
};

struct vlsctlc_ops {
    int (*pack_cmds)       (struct vlsctlc*, void*, int);
};

struct vlsctlc {
    int type;

    int bind_host_up;
    int bind_host_down;
    int bind_host_exit;
    int bind_host_dump;
    int bind_cfg_dump;

    int bind_join_node;
    struct sockaddr_in addr1;

    int bind_bogus_query;
    int queryId;
    struct sockaddr_in addr2;

    int bind_post_service;
    vsrvcHash hash1;
    struct sockaddr_in addr3;

    int bind_unpost_service;
    vsrvcHash hash2;
    struct sockaddr_in addr4;

    int bind_probe_service;
    vsrvcHash hash3;

    struct vlsctlc_ops *ops;
    struct vlsctlc_bind_ops* bind_ops;
};

int  vlsctlc_init  (struct vlsctlc*);
void vlsctlc_deinit(struct vlsctlc*);

#endif

