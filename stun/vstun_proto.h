#ifndef __VSTUN_PROTO_H__
#define __VSTUN_PROTO_H__

#define STUN_MAGIC ((uint32_t)0x2112A442)

enum {
    family_ipv4 = ((uint8_t)0x01),
    family_ipv6 = ((uint8_t)0x02)
};

enum {
    attr_mapped_addr     = ((uint16_t)0x0001),
    attr_response_addr   = ((uint16_t)0x0002),
    attr_change_request  = ((uint16_t)0x0003),
    attr_source_addr     = ((uint16_t)0x0004),
    attr_changed_addr    = ((uint16_t)0x0005),
    attr_username        = ((uint16_t)0x0006),
    attr_password        = ((uint16_t)0x0007),
    attr_msg_intgrity    = ((uint16_t)0x0008),
    attr_error_code      = ((uint16_t)0x0009),
    attr_unknown_attr    = ((uint16_t)0x000A),
    attr_reflected_from  = ((uint16_t)0x000B),
    attr_xor_mapped_addr = ((uint16_t)0x8020),
    attr_xor_only        = ((uint16_t)0x0021),
    attr_server_name     = ((uint16_t)0x8022)
};

enum {
    msg_bind_req         = ((uint16_t)0x0001),
    msg_bind_rsp         = ((uint16_t)0x0101),
    msg_bind_rsp_err     = ((uint16_t)0x0111),
};

struct vstun_msg_header {
    uint16_t type;
    uint16_t len;
    uint32_t magic;
    uint8_t  trans_id[12];
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

struct vattr_err_code {
    uint16_t pad;
    uint8_t  error_class;
    uint8_t  number;
    char reason[256];
};

struct vattr_unknown_attrs {
    uint16_t type[8];
    uint16_t nattrs;
};

struct vattr_string {
    char value[256];
};

struct vstun_msg {
    struct vstun_msg_header header;

    int has_mapped_addr;
    struct vattr_addrv4   mapped_addr;
    int has_error_code;
    struct vattr_err_code error_code;
    int has_server_name;
    struct vattr_string   server_name;
    int has_unkown_attrs;
    struct vattr_unknown_attrs unknown_attrs;
};

struct vstun_proto_ops {
    int (*encode)(struct vstun_msg*, char*, int);
    int (*decode)(char*, int, struct vstun_msg*);
};

int vsockaddr_to_addrv4(struct sockaddr_in*, struct vattr_addrv4*);
int vsockaddr_from_addrv4(struct vattr_addrv4*, struct sockaddr_in*);

#endif

