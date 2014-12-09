#ifndef __VSTUN_H__
#define __VSTUN_H__

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
    //todo;
};

struct vattr_header {
    uint16_t type;
    uint16_t len;
};

struct vattr_addr4 {
    uint8_t  pad;
    uint8_t  family;
    uint16_t port;
    uint32_t addr;
};

struct vattr_chg_req {
    uint32_t value;
};

struct vattr_error {
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

union vattr_template {
    struct vattr_addr4   addr;
    struct vattr_chg_req req;
    struct vattr_error   err;
    struct vattr_string  str;
    struct vattr_integrity integrity;
    struct vattr_unknown_attrs unknown_attrs;
};

struct vstun_msg {
    struct vstun_msg_header hdr;
    bool has_mapped_addr;
    struct vattr_addr4 mapped_addr;

    bool has_rsp_addr;
    struct vattr_addr4 rsp_addr;

    bool has_chg_req;
    struct vattr_chg_req chg_req;

    bool has_source_addr;
    struct vattr_addr4 src_addr;

    bool has_chged_addr;
    struct vattr_addr4 chged_addr;

    bool has_username;
    struct vattr_string username;

    bool has_passwd;
    struct vattr_string passwd;

    bool has_msg_integrity;
    struct vattr_integrity intergrity;

    bool has_err_code;
    struct vattr_error err_code;

    bool has_unknown_attrs;
    struct vattr_unknown_attrs unknown_attrs;

    bool has_reflected_from;
    struct vattr_addr4 reflected_from;

    bool has_xor_mapped_addr;
    struct vattr_addr4 xor_mapped_addr;

    bool has_xor_only;
    bool has_server_name;
    struct vattr_string server_name;

    bool has_2snd_addr;
    struct vattr_addr4 secondary_addr;
};

struct vstun;
struct vstun_ops {
    int (*decode)(struct vstun*, char*, int, struct vstun_msg*);
};

struct vstun {
    struct vstun_ops* ops;
};

int  vstun_init  (struct vstun*);
void vstun_deinit(struct vstun*);

#endif
