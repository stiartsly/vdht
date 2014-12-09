#include "vglobal.h"
#include "vstun.h"

struct vattr_parse_routine {
    uint16_t attr;
    int (*parse_cb)(char*, int, struct vstun_msg*);
};

static
int _aux_attr_parse_addr(char* buf, int len, struct vattr_addr4* addr)
{
    int sz  = 0;

    vassert(addr);
    vassert(buf);
    vassert(len != 8);

    addr->pad = 0;
    addr->family = get_uint8(offset_addr(buf, sz));
    sz += sizeof(uint16_t);
    addr->port = get_uint16(offset_addr(buf, sz));
    addr->port = ntohs(addr->port);
    sz += sizeof(uint16_t);
    addr->addr = get_uint32(offset_addr(buf, sz));
    addr->addr = ntohl(addr->addr);
    return 0;
}

static
int _aux_attr_parse_string(char* buf, int len, struct vattr_string* str)
{
    vassert(str);
    vassert(buf);
    vassert(len > 0);

    //todo;
    return 0;
}

static
int _vattr_parse_mapped_addr(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_mapped_addr = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->mapped_addr);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_response_addr(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;
    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_rsp_addr = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->rsp_addr);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_change_req(char* buf, int len, struct vstun_msg* msg)
{
    vassert(buf);
    vassert(len);
    vassert(msg);

    retE((len != 4));
    msg->has_chg_req = 1;
    msg->chg_req.value = get_uint32(buf);
    msg->chg_req.value = ntohl(msg->chg_req.value);
    return 0;
}

static
int _vattr_parse_source_addr(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_source_addr = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->src_addr);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_changed_addr(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_chged_addr = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->chged_addr);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_username(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    msg->has_username = 1;
    ret = _aux_attr_parse_string(buf, len, &msg->username);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_passwd(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    msg->has_passwd = 1;
    ret = _aux_attr_parse_string(buf, len, &msg->passwd);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_integrity(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    msg->has_msg_integrity = 1;
    //todo;
    return 0;
}


static
int _vattr_parse_error(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(attr);
    vassert(buf);
    vassert(len > 0);

    msg->has_err_code = 1;
    //todo;
    return 0;
}

static
int _vattr_parse_reflected_from(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;
    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_reflected_from = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->reflected_from);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_xor_mapped_addr(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;
    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_xor_mapped_addr = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->xor_mapped_addr);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_server_name(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;

    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    msg->has_server_name = 1;
    ret = _aux_attr_parse_string(buf, len, &msg->server_name);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_secondary_addr(char* buf, int len, struct vstun_msg* msg)
{
    int ret = 0;
    vassert(buf);
    vassert(len > 0);
    vassert(msg);

    retE((len != 8));
    msg->has_2snd_addr = 1;
    ret = _aux_attr_parse_addr(buf, len, &msg->secondary_addr);
    retE((ret < 0));
    return 0;
}

static
int _vattr_parse_xor_only(char* buf, int len, struct vstun_msg* msg)
{
    vassert(buf);
    vassert(msg);

    msg->has_xor_only = 1;
    return 0;
}


static
int _vattr_parse_unknown_attrs(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    //todo;
    return 0;
}

static
struct vattr_parse_routine attr_parse_routines[] = {
    {attr_mapped_addr,     _vattr_parse_mapped_addr     },
    {attr_rsp_addr   ,     _vattr_parse_response_addr   },
    {attr_chg_req    ,     _vattr_parse_change_req      },
    {attr_source_addr,     _vattr_parse_response_addr   },
    {attr_changed_addr,    _vattr_parse_changed_addr    },
    {attr_username,        _vattr_parse_username        },
    {attr_password,        _vattr_parse_passwd          },
    {attr_msg_intgrity,    _vattr_parse_integrity       },
    {attr_err_code    ,    _vattr_parse_error           },
    {attr_reflected_from,  _vattr_parse_reflected_from  },
    {attr_xor_mapped_addr, _vattr_parse_xor_mapped_addr },
    {attr_server_name,     _vattr_parse_server_name     },
    {attr_second_addr,     _vattr_parse_secondary_addr  },
    {attr_xor_only,        _vattr_parse_xor_only        },
    {0, 0}
};

static
int _vstun_msg_decode(struct vstun* stun, char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_parse_routine* routine = attr_parse_routines;
    char* data = NULL;
    int sz = 0;

    vassert(stun);
    vassert(buf);
    vassert(len > 0);
    vassert(msg);
    retE((len <= sizeof(struct vstun_msg_header));

    memset(msg, 0, sizeof(*msg));
    memcpy(&msg->hdr, buf, sizeof(msg->hdr));
    msg->hdr.type = ntohs(msg->hdr.type);
    msg->hdr.len  = ntohs(msg->hdr.len);

    data = offset_addr(buf, sizeof(struct vstun_msg_header));
    sz   = msg.hdr.len;
    while (sz > 0) {
        struct vattr_header attr;
        int ret = 0;

        memcpy(&attr, data, sizeof(attr));
        attr.type = ntohs(attr.type);
        attr.len  = ntohs(attr.len);

        data = offset_addr(data, sizeof(attr));
        sz  -= sizeof(attr);

        for (; routine->parse_cb; routine++) {
            if (routine->attr != attr.type) {
                continue;
            }
            ret = routine->parse_cb(data, attr.len, msg);
            retE((ret < 0));
        }
        //todo : unknown attr;
        data = offset_addr(data, attr.len);
        sz  -= attr.len;
    }
    return 0;
}

struct vstun_ops stun_ops = {
    .decode = _vstun_msg_decode
};

static
int _aux_stun_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstun* stun = (struct vstun*)cookie;
    struct vstun_msg msg;
    int ret = 0;
    vassert(stun);
    vassert(mu);

    ret = stun->ops->decode(stun, mu->data, mu->len, &msg);
    retE((ret < 0));
    return 0;
}

int vstun_init  (struct vstun* stun)
{
    vassert(stun);

    //todo;
    stun->ops = &stun_ops;
    return 0;
}

void vstun_deinit(struct vstun* stun)
{
    vassert(stun);
    //todo;
    return ;
}

