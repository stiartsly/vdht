#ifndef __VSTUN_H__
#define __VSTUN_H__

#include "vrpc.h"
#include "vmsger.h"

enum {
    attr_mapped_addr  = ((uint16_t)0x0001),
    attr_rsp_addr     = ((uint16_t)0x0002),
    attr_chg_req      = ((uint16_t)0x0003),
    attr_source_addr  = ((uint16_t)0x0004),
    attr_changed_addr = ((uint16_t)0x0005),
    attr_username     = ((uint16_t)0x0006),
    attr_password     = ((uint16_t)0x0007),
    attr_msg_intgrity = ((uint16_t)0x0008),
    attr_err_code     = ((uint16_t)0x0009),
    attr_unknown_attr = ((uint16_t)0x000A),
    attr_reflected_from  = ((uint16_t)0x000B),
    attr_xor_mapped_addr = ((uint16_t)0x8020),
    attr_xor_only     = ((uint16_t)0x0021),
    attr_server_name  = ((uint16_t)0x8022),
    attr_second_addr  = ((uint16_t)0x8050) // non standard extension.
};

enum {
    msg_bind_req = ((uint16_t)0x0001),
    msg_bind_rsp = ((uint16_t)0x0101),
    msg_bind_rsp_err          = ((uint16_t)0x0111),
    msg_shared_secret_req     = ((uint16_t)0x0002),
    msg_shared_secret_rsp     = ((uint16_t)0x0102),
    msg_shared_secret_rsp_err = ((uint16_t)0x0112)
};

struct vstun_msg_header {
    uint16_t type;
    uint16_t len;
    uint8_t  token[16];
};

struct vattr_header {
    uint16_t type;
    uint16_t len;
};

struct vattr_addrv4 {
    uint8_t  pad;
    uint8_t  family;
    uint16_t port;
    uint32_t addr;
};

struct vattr_change_req {
    uint32_t value;
};

struct vattr_error_code {
    uint16_t pad;
    uint8_t  error_class;
    uint8_t  number;
    char reason[256];
    uint16_t reason_sz;
};

struct vattr_unknown_attrs {
    uint16_t type[8];
    uint16_t nattrs;
};

struct vattr_string {
    char value[256];
    uint16_t val_sz;
};

struct vattr_integrity {
    char hash[20];
};

struct vstun_msg {
    struct vstun_msg_header header;

    int has_mapped_addr;
    struct vattr_addrv4 mapped_addr;
    int has_resp_addr;
    struct vattr_addrv4 resp_addr;
    int has_change_req;
    struct vattr_change_req change_req;
    int has_source_addr;
    struct vattr_addrv4 source_addr;
    int has_changed_addr;
    struct vattr_addrv4 changed_addr;
    int has_username;
    struct vattr_string username;
    int has_passwd;
    struct vattr_string passwd;
    int has_msg_integrity;
    struct vattr_integrity intergrity;
    int has_error_code;
    struct vattr_error_code error_code;
    int has_unkown_attrs;
    struct vattr_unknown_attrs unknown_attrs;
    int has_reflected_from;
    struct vattr_addrv4 reflected_from;
    int has_xor_mapped_addr;
    struct vattr_addrv4 xor_mapped_addr;
    int has_xor_only;
    int has_server_name;
    struct vattr_string server_name;
    int has_second_addr;
    struct vattr_addrv4 second_addr;
};

struct vstun;
struct vstun_ops {
    int (*encode_msg)(struct vstun*, char*, int);
    int (*decode_msg)(struct vstun*, char*, int);
    int (*proc_msg)  (struct vstun*);
};

struct vstun {
    int has_source_addr;
    struct vattr_addrv4 source;
    int has_alt_addr;
    struct vattr_addrv4 alt;
    int has_second_addr;
    struct vattr_addrv4 second;

    struct vattr_addrv4 from;
    struct vstun_msg req;
    struct vstun_msg rsp;

    struct vrpc    rpc;
    struct vmsger  msger;
    struct vwaiter waiter;

    struct vstun_ops* ops;
};

struct vstun* vstun_create(struct vconfig*);
void vstun_destroy(struct vstun*);

#endif

