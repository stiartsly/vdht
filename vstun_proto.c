#include "vglobal.h"
#include "vstun_proto.h"

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
int _vstun_proto_encode(struct vstun_msg* msg, char* buf, int len)
{
    struct vstun_msg_header* hdr = (struct vstun_msg_header*)buf;
    struct vattr_enc_routine* encoder = attr_enc_routines;
    int ret = 0;
    int sz  = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    memcpy(hdr, &msg->header, sizeof(*hdr));
    hdr->type  = htons(hdr->type);
    hdr->len   = htons(hdr->len);
    hdr->magic = htonl(STUN_MAGIC);
    sz += sizeof(*hdr);

    for (; encoder->enc_cb; encoder++) {
        ret = encoder->enc_cb(msg, buf + sz, len - sz);
        retE((ret < 0));
        sz  += ret;
    }
    return sz;
}

static
int _vstun_proto_decode(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_dec_routine* decoder = attr_dec_routines;
    struct vstun_msg_header* hdr = &msg->header;
    char* data = NULL;
    int ret = 0;
    int sz  = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);
    retE((len <= sizeof(*hdr)));

    memcpy(hdr, buf, sizeof(*hdr));
    hdr->type  = ntohs(hdr->type);
    hdr->len   = ntohs(hdr->len);
    hdr->magic = ntohl(hdr->magic);

    data = offset_addr(buf, sizeof(*hdr));
    sz   = hdr->len;
    while (sz > 0) {
        struct vattr_header attr;
        attr.type = ntohs(attr.type);
        attr.len  = ntohs(attr.len);

        data = offset_addr(data, sizeof(attr));
        sz  -= sizeof(attr);

        for (; decoder->attr != attr_unknown_attr; decoder++) {
            if (decoder->attr == attr.type) {
                break;
            }
        }
        ret = decoder->dec_cb(data, attr.len, msg);
        retE((ret < 0));
        data = offset_addr(data, attr.len);
        sz  -= attr.len;
    }
    return 0;
}

struct vstun_proto_ops stun_proto_ops = {
    .encode = _vstun_proto_encode,
    .decode = _vstun_proto_decode
};

int vsockaddr_to_addrv4(struct sockaddr_in* sin_addr, struct vattr_addrv4* attr_addr)
{
    int ret = 0;
    vassert(sin_addr);
    vassert(attr_addr);

    ret = vsockaddr_unconvert2(sin_addr, &attr_addr->addr, &attr_addr->port);
    retE((ret < 0));
    attr_addr->family = family_ipv4;
    attr_addr->pad    = 0;

    return 0;

}
int vsockaddr_from_addrv4(struct vattr_addrv4* attr_addr, struct sockaddr_in* sin_addr)
{
    int ret = 0;
    vassert(attr_addr);
    vassert(sin_addr);

    retE((attr_addr->family != family_ipv4));
    ret = vsockaddr_convert2(attr_addr->addr, attr_addr->port, sin_addr);
    retE((ret < 0));
    return 0;
}

