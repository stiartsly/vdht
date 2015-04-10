#ifndef __VLSCTL_H__
#define __VLSCTL_H__

#include "vrpc.h"
#include "vmsger.h"
#include "vhost.h"

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
    vlsctl_rsp_err       = (uint8_t)0b11
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

struct vlsctl;
typedef int (*vlsctl_exec_cmd_t)(struct vlsctl*, void*, int);
struct vlsctl_exec_cmd_desc {
    const char* desc;
    uint32_t cmd_id;
    vlsctl_exec_cmd_t cmd;
};

struct vlsctl_ops {
    int (*unpack_cmds)(struct vlsctl*, void*, int);
};

struct vlsctl {
    struct vrpc      rpc;
    struct vmsger    msger;
    struct vhost*    host;

    struct vsockaddr   addr;
    struct vlsctl_ops* ops;
};

int  vlsctl_init  (struct vlsctl*, struct vhost*, struct vconfig*);
void vlsctl_deinit(struct vlsctl*);

#endif

