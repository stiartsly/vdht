#ifndef __VLSCTL_H__
#define __VLSCTL_H__

#include "vrpc.h"
#include "vmsger.h"
#include "vhost.h"

/*
 * lsctl message structure:
 * @message length only includes fields of "command Id", "command legth", and "comand content"
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

struct vlsctl;
struct vlsctl_exec_cmd_desc {
    const char* desc;
    uint32_t cmd_id;
    int (*cmd)(struct vlsctl*, void*, int, struct vsockaddr*);
};

struct vlsctl_ops {
    int (*unpack_cmd)(struct vlsctl*, void*, int, struct vsockaddr*);
    int (*pack_cmd)  (struct vlsctl*, void*, int, void*);
};

typedef int (*vlsctl_pack_cmd_t)(void*, int, void*);
struct vlsctl_rsp_args {
    uint32_t cmd_id;
    union _rsp_args {
        struct find_service_rsp_args {
            vlsctl_pack_cmd_t pack_cb;
            int total;
            vsrvcHash hash;
            int proto;
            int index;
            struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
        } find_service_rsp_args;

        struct probe_service_rsp_args {
            vlsctl_pack_cmd_t pack_cb;
            struct vlsctl* lsctl;
            struct vsockaddr from;

            int total;
            vsrvcHash hash;
            int proto;
            int index;
            struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
        } probe_service_rsp_args;

        struct error_rsp_args {
            int err_val;
        } error_rsp_args;
    } uargs;
};

struct vlsctl_pack_cmd_ops {
    int (*find_service_rsp) (void*, int, void*);
    int (*probe_service_rsp)(void*, int, void*);
};

struct vlsctl {
    struct vrpc      rpc;
    struct vmsger    msger;

    struct vsockaddr addr;
    struct vlsctl_ops* ops;
    struct vlsctl_pack_cmd_ops* pack_cmd_ops;
};

int  vlsctl_init  (struct vlsctl*, struct vconfig*);
void vlsctl_deinit(struct vlsctl*);

#endif

