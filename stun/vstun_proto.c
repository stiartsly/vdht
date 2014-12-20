#include "vglobal.h"
#include "vstun.h"

struct vattr_enc_routine {
    uint16_t attr;
    int (*enc_cb)(struct vstun_msg*, char*, int);
};

struct vattr_dec_routine {
    uint16_t attr;
    int (*dec_cb)(char*, int, struct vstun_msg*);
};

struct vmsg_proc_routine {
    uint16_t mtype;
    int (*proc_cb)(struct vstun_msg*, void*);
};

static
void vattr_addrv4_enc(struct vattr_addrv4* addr, char* buf, int len)
{
    int sz = 0;
    vassert(addr);
    vassert(buf);
    vassert(len == 8);

    set_uint8(offset_addr(buf, sz), addr->pad);
    sz += sizeof(uint8_t);
    set_uint8(offset_addr(buf, sz), addr->family);
    sz += sizeof(uint8_t);
    addr->port = htons(addr->port);
    addr->addr = htonl(addr->addr);
    set_uint16(offset_addr(buf, sz), addr->port);
    sz += sizeof(uint16_t);
    set_uint32(offset_addr(buf, sz), addr->port);

    return ;
}

static
void vattr_addrv4_dec(char* buf, int len, struct vattr_addrv4* addr)
{
    int sz = 0;
    vassert(buf);
    vassert(addr);
    vassert(len == 8);

    addr->pad = 0;
    sz += sizeof(uint8_t);
    addr->family = get_uint8(offset_addr(buf, sz));
    sz += sizeof(uint8_t);
    addr->port = get_uint16(offset_addr(buf, sz));
    sz += sizeof(uint16_t);
    addr->addr = get_uint32(offset_addr(buf, sz));
    addr->port = ntohs(addr->port);
    addr->addr = ntohl(addr->addr);
    return ;
}

static
int vattr_string_dec(char* buf, int len, struct vattr_string* str)
{
    vassert(str);
    vassert(buf);
    vassert(len > 0);

    //todo;
    return 0;
}

int _vattr_enc_mapped_addr(struct vstun_msg* msg, char* buf, int len)
{
    struct vattr_header* attr = (struct vattr_header*)buf;
    struct vattr_addrv4* addr = &msg->mapped_addr;
    int sz = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    retS((!msg->has_mapped_addr));
    attr->type = attr_mapped_addr;
    attr->len  = 8;
    sz += sizeof(*attr);

    vattr_addrv4_enc(addr, (char*)(attr + 1), attr->len);
    attr->type = htons(attr->type);
    attr->len  = htons(attr->len);
    sz += attr->len;

    return sz;
}

static
int _vattr_enc_error_code(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_server_name(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_unknown_attrs(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_todo_attrs(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
struct vattr_enc_routine attr_enc_routines[] = {
    {attr_mapped_addr,     _vattr_enc_mapped_addr  },
    {attr_error_code,      _vattr_enc_error_code   },
    {attr_server_name,     _vattr_enc_server_name  },
    {attr_response_addr,   _vattr_enc_todo_attrs   },
    {attr_changed_addr,    _vattr_enc_todo_attrs   },
    {attr_source_addr ,    _vattr_enc_todo_attrs   },
    {attr_changed_addr,    _vattr_enc_todo_attrs   },
    {attr_username,        _vattr_enc_todo_attrs   },
    {attr_password,        _vattr_enc_todo_attrs   },
    {attr_msg_intgrity,    _vattr_enc_todo_attrs   },
    {attr_reflected_from,  _vattr_enc_todo_attrs   },
    {attr_xor_mapped_addr, _vattr_enc_todo_attrs   },
    {attr_xor_only,        _vattr_enc_todo_attrs   },
    {attr_unknown_attr,    _vattr_enc_unknown_attrs},
    {0, 0}
};

static
int _vattr_dec_mapped_addr(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->mapped_addr;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_mapped_addr = 1;
    vattr_addrv4_dec(buf, len, addr);
    return 0;
}

static
int _vattr_dec_error_code(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    msg->has_error_code = 1;
    //todo;
    return 0;
}

static
int _vattr_dec_server_name(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    msg->has_server_name = 1;
    ret = vattr_string_dec(buf, len, &msg->server_name);
    retE((ret < 0));
    return 0;
}

static
int _vattr_dec_unknown_attrs(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    //todo;
    return 0;
}

static
int _vattr_dec_todo_attrs(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
struct vattr_dec_routine attr_dec_routines[] = {
    {attr_mapped_addr,     _vattr_dec_mapped_addr  },
    {attr_error_code,      _vattr_dec_error_code   },
    {attr_server_name,     _vattr_dec_server_name  },
    {attr_response_addr,   _vattr_dec_todo_attrs   },
    {attr_change_request , _vattr_dec_todo_attrs   },
    {attr_source_addr,     _vattr_dec_todo_attrs   },
    {attr_changed_addr,    _vattr_dec_todo_attrs   },
    {attr_username,        _vattr_dec_todo_attrs   },
    {attr_password,        _vattr_dec_todo_attrs   },
    {attr_msg_intgrity,    _vattr_dec_todo_attrs   },
    {attr_reflected_from,  _vattr_dec_todo_attrs   },
    {attr_xor_mapped_addr, _vattr_dec_todo_attrs   },
    {attr_xor_only,        _vattr_dec_todo_attrs   },
    {attr_unknown_attr,    _vattr_dec_unknown_attrs},
    {0, 0}
};

static
int _vmsg_proc_bind_req(struct vstun_msg* msg, void* argv)
{
    varg_decl(argv, 0, struct vstun_msg*,   rsp );
    varg_decl(argv, 1, struct sockaddr_in*, from);
    varg_decl(argv, 2, char*, server_name);
    struct vstun_msg_header* hdr = &rsp->header;

    vassert(msg);
    vassert(argv);

    memset(rsp, 0, sizeof(*rsp));
    hdr->type = htons(msg_bind_rsp);
    memcpy(hdr->token, msg->header.token, 16);

    {
        struct vattr_addrv4* addr = &rsp->mapped_addr;
        rsp->has_mapped_addr = 1;
        addr->pad = 0;
        addr->family = family_ipv4;
        vsockaddr_unconvert2(from, &addr->addr, &addr->port);
        addr->addr = htonl(addr->addr);
        addr->port = htons(addr->port);
    }
    {
        struct vattr_string* name = &rsp->server_name;
        rsp->has_server_name = 1;
        strcpy(name->value, server_name);
    }
    return 0;
}

static
int _vmsg_proc_bind_rsp_suc(struct vstun_msg* msg, void* argv)
{
    vassert(msg);

    //todo;
    return 1;
}

static
int _vmsg_proc_bind_rsp_err(struct vstun_msg* msg, void* argv)
{
    vassert(msg);

    //todo;
    return 1;
}

static
struct vmsg_proc_routine msg_proc_routines[] = {
    { msg_bind_req,     _vmsg_proc_bind_req     },
    { msg_bind_rsp,     _vmsg_proc_bind_rsp_suc },
    { msg_bind_rsp_err, _vmsg_proc_bind_rsp_err },
    { 0, 0 }
};

static
int _vstun_msg_enc(struct vstun* stun, struct vstun_msg* msg, char* buf, int len)
{
    struct vstun_msg_header* hdr = (struct vstun_msg_header*)buf;
    struct vattr_enc_routine* routine = attr_enc_routines;
    int ret = 0;
    int sz  = 0;

    vassert(stun);
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    memcpy(hdr, &msg->header, sizeof(*hdr));
    hdr->type = ntohs(hdr->type);
    hdr->len  = ntohs(hdr->len);
    sz += sizeof(*hdr);

    for (; routine->enc_cb; routine++) {
        ret = routine->enc_cb(msg, buf + sz, len - sz);
        retE((ret < 0));
        sz  += ret;
    }
    return sz;
}

static
int _vstun_msg_dec(struct vstun* stun, char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_dec_routine* routine = attr_dec_routines;
    struct vstun_msg_header* hdr = &msg->header;
    char* data = NULL;
    int ret = 0;
    int sz  = 0;

    vassert(stun);
    vassert(msg);
    vassert(buf);
    vassert(len > 0);
    retE((len <= sizeof(*hdr)));

    memcpy(hdr, buf, sizeof(*hdr));
    hdr->type = ntohs(hdr->type);
    hdr->len  = ntohs(hdr->len);

    data = offset_addr(buf, sizeof(*hdr));
    sz   = hdr->len;
    while (sz > 0) {
        struct vattr_header attr;

        memcpy(&attr, data, sizeof(attr));
        attr.type = ntohs(attr.type);
        attr.len  = ntohs(attr.len);

        data = offset_addr(data, sizeof(attr));
        sz  -= sizeof(attr);

        for (; routine->attr != attr_unknown_attr; routine++) {
            if (routine->attr == attr.type) {
                break;
            }
        }
        ret = routine->dec_cb(data, attr.len, msg);
        retE((ret < 0));
        data = offset_addr(data, attr.len);
        sz  -= attr.len;
    }
    return 0;
}

static
int _vstun_msg_proc(struct vstun* stun, struct vstun_msg* msg, void* argv)
{
    struct vmsg_proc_routine* routine = msg_proc_routines;
    int ret = 0;

    vassert(stun);
    vassert(msg);

    for (; routine->proc_cb; routine++) {
        if (routine->mtype == msg->header.type) {
            break;
        }
    }
    retE((!routine->proc_cb));
    ret = routine->proc_cb(msg, argv);
    retE((ret < 0));
    return 0;
}

struct vstun_msg_ops stun_msg_ops = {
    .encode = _vstun_msg_enc,
    .decode = _vstun_msg_dec,
    .handle = _vstun_msg_proc
};

