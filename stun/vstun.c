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
    int (*proc_cb)(struct vstun*);
};

static
void vattr_addrv4_init(struct vattr_addrv4* addr, struct sockaddr_in* s_addr)
{
    vassert(addr);
    vassert(s_addr);

    addr->family = family_ipv4;
    addr->pad    = 0;
    vsockaddr_unconvert2(s_addr, &addr->addr, &addr->port);
    return ;
}

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
void vattr_addrv4_copy(struct vattr_addrv4* dest, struct vattr_addrv4* src)
{
    vassert(dest);
    vassert(src);

    memset(dest, 0,   sizeof(*dest));
    memcpy(dest, src, sizeof(*dest));
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
int _vattr_enc_response_addr(struct vstun_msg* msg, char* buf, int len)
{
    struct vattr_header* attr = (struct vattr_header*)buf;
    struct vattr_addrv4* addr = &msg->resp_addr;
    int sz = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    retS((!msg->has_resp_addr));
    attr->type = attr_rsp_addr;
    attr->len  = 8;
    sz += sizeof(*attr);

    vattr_addrv4_enc(addr, (char*)(buf + 1), attr->len);
    attr->type = htons(attr->type);
    attr->len  = htons(attr->len);
    sz += attr->len;

    return sz;
}

static
int _vattr_enc_change_req(struct vstun_msg* msg, char* buf, int len)
{
    struct vattr_header* attr = (struct vattr_header*)buf;
    struct vattr_change_req* req = &msg->change_req;
    int sz = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    retS((!msg->has_change_req));
    attr->type = attr_changed_addr;
    attr->len  = 4;
    sz += sizeof(*attr);

    set_uint32(offset_addr(buf, sz), htonl(req->value));
    sz += sizeof(uint32_t);

    return sz;
}

static
int _vattr_enc_source_addr(struct vstun_msg* msg, char* buf, int len)
{
    struct vattr_header* attr = (struct vattr_header*)buf;
    struct vattr_addrv4* addr = &msg->source_addr;
    int sz = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    retS((!msg->has_source_addr));
    attr->type = attr_source_addr;
    attr->len  = 8;
    sz += sizeof(*attr);

    vattr_addrv4_enc(addr, (char*)(attr + 1), attr->len);
    attr->type = htons(attr->type);
    attr->len  = htons(attr->len);
    sz += attr->len;

    return sz;
}

static
int _vattr_enc_changed_addr(struct vstun_msg* msg, char* buf, int len)
{
    struct vattr_header* attr = (struct vattr_header*)buf;
    struct vattr_addrv4* addr = &msg->changed_addr;
    int sz = 0;

    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    retS((!msg->has_changed_addr));
    attr->type = attr_source_addr;
    attr->len  = 8;
    sz += sizeof(*attr);

    vattr_addrv4_enc(addr, (char*)(attr + 1), attr->len);
    attr->type = htons(attr->type);
    attr->len  = htons(attr->len);
    sz += attr->len;

    return sz;
}

static
int _vattr_enc_username(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_password(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_msg_integrity(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
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
int _vattr_enc_reflected_from(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_xor_mapped_addr(struct vstun_msg* msg, char* buf, int len)
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
int _vattr_enc_second_addr(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_xor_only(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
int _vattr_enc_unkown_attrs(struct vstun_msg* msg, char* buf, int len)
{
    vassert(msg);
    vassert(buf);

    //todo;
    return 0;
}

static
struct vattr_enc_routine attr_enc_routines[] = {
    {attr_mapped_addr,     _vattr_enc_mapped_addr    },
    {attr_rsp_addr   ,     _vattr_enc_response_addr  },
    {attr_changed_addr,    _vattr_enc_change_req     },
    {attr_source_addr ,    _vattr_enc_source_addr    },
    {attr_changed_addr,    _vattr_enc_changed_addr   },
    {attr_username,        _vattr_enc_username       },
    {attr_password,        _vattr_enc_password       },
    {attr_msg_intgrity,    _vattr_enc_msg_integrity  },
    {attr_err_code,        _vattr_enc_error_code     },
    {attr_reflected_from,  _vattr_enc_reflected_from },
    {attr_xor_mapped_addr, _vattr_enc_xor_mapped_addr},
    {attr_server_name,     _vattr_enc_server_name    },
    {attr_second_addr,     _vattr_enc_second_addr    },
    {attr_xor_only,        _vattr_enc_xor_only       },
    {attr_unknown_attr,    _vattr_enc_unkown_attrs   },
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
int _vattr_dec_response_addr(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->resp_addr;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_resp_addr = 1;
    vattr_addrv4_dec(buf, len, addr);
    return 0;
}

static
int _vattr_dec_change_req(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_change_req* req = &msg->change_req;
    vassert(buf);
    vassert(msg);

    retE((len != 4));
    msg->has_change_req = 1;
    req->value = get_uint32(buf);
    req->value = ntohl(req->value);
    return 0;
}

static
int _vattr_dec_source_addr(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->source_addr;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_source_addr = 1;
    vattr_addrv4_dec(buf, len, addr);
    return 0;
}

static
int _vattr_dec_changed_addr(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->changed_addr;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_changed_addr = 1;
    vattr_addrv4_dec(buf, len, addr);
    return 0;
}

static
int _vattr_dec_username(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_string* name = &msg->username;
    int ret = 0;

    vassert(buf);
    vassert(msg);

    msg->has_username = 1;
    ret = vattr_string_dec(buf, len, name);
    retE((ret < 0));
    return 0;
}

static
int _vattr_dec_passwd(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_string* pswd = &msg->passwd;
    int ret = 0;

    vassert(buf);
    vassert(msg);

    msg->has_passwd = 1;
    ret = vattr_string_dec(buf, len, pswd);
    retE((ret < 0));
    return 0;
}

static
int _vattr_dec_integrity(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    msg->has_msg_integrity = 1;
    //todo;
    return 0;
}


static
int _vattr_dec_error(char* buf, int len, struct vstun_msg* msg)
{
    vassert(msg);
    vassert(buf);
    vassert(len > 0);

    msg->has_error_code = 1;
    //todo;
    return 0;
}

static
int _vattr_dec_reflected_from(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->reflected_from;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_reflected_from = 1;
    vattr_addrv4_dec(buf, len, addr);
    return 0;
}

static
int _vattr_dec_xor_mapped_addr(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->xor_mapped_addr;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_xor_mapped_addr = 1;
    vattr_addrv4_dec(buf, len, addr);
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
int _vattr_dec_secondary_addr(char* buf, int len, struct vstun_msg* msg)
{
    struct vattr_addrv4* addr = &msg->second_addr;
    vassert(buf);
    vassert(msg);

    retE((len != 8));
    msg->has_second_addr = 1;
    vattr_addrv4_dec(buf, len, addr);
    return 0;
}

static
int _vattr_dec_xor_only(char* buf, int len, struct vstun_msg* msg)
{
    vassert(buf);
    vassert(msg);

    msg->has_xor_only = 1;
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
struct vattr_dec_routine attr_dec_routines[] = {
    {attr_mapped_addr,     _vattr_dec_mapped_addr     },
    {attr_rsp_addr   ,     _vattr_dec_response_addr   },
    {attr_chg_req    ,     _vattr_dec_change_req      },
    {attr_source_addr,     _vattr_dec_source_addr     },
    {attr_changed_addr,    _vattr_dec_changed_addr    },
    {attr_username,        _vattr_dec_username        },
    {attr_password,        _vattr_dec_passwd          },
    {attr_msg_intgrity,    _vattr_dec_integrity       },
    {attr_err_code    ,    _vattr_dec_error           },
    {attr_reflected_from,  _vattr_dec_reflected_from  },
    {attr_xor_mapped_addr, _vattr_dec_xor_mapped_addr },
    {attr_server_name,     _vattr_dec_server_name     },
    {attr_second_addr,     _vattr_dec_secondary_addr  },
    {attr_xor_only,        _vattr_dec_xor_only        },
    {attr_unknown_attr,    _vattr_dec_unknown_attrs   },
    {0, 0}
};

static
int vmsg_proc_bind_req(struct vstun* stun)
{
    struct vstun_msg* req = &stun->req;
    struct vstun_msg* rsp = &stun->rsp;
    vassert(stun);

    memset(rsp, 0, sizeof(*rsp));
    rsp->header.type = msg_bind_rsp;
    memcpy(rsp->header.token, req->header.token, 16);
    if (req->has_xor_only) {
        //todo;
    } else {
        rsp->has_xor_only = 0;
        rsp->has_mapped_addr = 1;
        vattr_addrv4_copy(&rsp->mapped_addr, &stun->from);
    }
    if (stun->has_source_addr) {
        rsp->has_source_addr = 1;
        vattr_addrv4_copy(&rsp->source_addr, &stun->source);
    }
    if (stun->has_alt_addr) {
        rsp->has_changed_addr = 1;
        vattr_addrv4_copy(&rsp->changed_addr, &stun->alt);
    }
    if (stun->has_second_addr) {
        rsp->has_second_addr = 1;
        vattr_addrv4_copy(&rsp->second_addr, &stun->second);
    }
    return 0;
}

static
struct vmsg_proc_routine msg_proc_routines[] = {
    { msg_bind_req, vmsg_proc_bind_req },
    { 0, 0 }
};

static
int _vstun_msg_enc(struct vstun* stun, char* buf, int len)
{
    struct vattr_enc_routine* routine = attr_enc_routines;
    struct vstun_msg_header* hdr = (struct vstun_msg_header*)buf;
    struct vstun_msg* rsp = &stun->rsp;
    int ret = 0;
    int sz  = 0;

    vassert(stun);
    vassert(buf);
    vassert(len > 0);

    memcpy(hdr, &rsp->header, sizeof(*hdr));
    hdr->type = ntohs(hdr->type);
    hdr->len  = ntohs(hdr->len);
    sz += sizeof(*hdr);

    for (; routine->enc_cb; routine++) {
        ret = routine->enc_cb(rsp, buf + sz, len - sz);
        retE((ret < 0));
        sz  += ret;
    }
    return sz;
}

static
int _vstun_msg_dec(struct vstun* stun, char* buf, int len)
{
    struct vattr_dec_routine* routine = attr_dec_routines;
    struct vstun_msg* req        = &stun->req;
    struct vstun_msg_header* hdr = &req->header;
    char* data = NULL;
    int ret = 0;
    int sz  = 0;

    vassert(stun);
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
        ret = routine->dec_cb(data, attr.len, req);
        retE((ret < 0));
        data = offset_addr(data, attr.len);
        sz  -= attr.len;
    }
    return 0;
}

static
int _vstun_msg_proc(struct vstun* stun)
{
    struct vmsg_proc_routine* routine = msg_proc_routines;
    struct vstun_msg* req = &stun->req;
    int ret = 0;

    vassert(stun);
    for (; routine->proc_cb; routine++) {
        if (routine->mtype == req->header.type) {
            break;
        }
    }
    retE((!routine->proc_cb));

    ret = routine->proc_cb(stun);
    retE((ret < 0));
    return 0;
}

static
int _vstun_render_srvc(struct vstun* stun)
{
    struct vattr_addrv4* source = &stun->source;
    struct vhost* host = stun->host;
    struct sockaddr_in addr;
    int ret = 0;
    vassert(stun);

    vsockaddr_convert2(source->addr, source->port, &addr);
    ret = host->ops->plug(host, PLUGIN_STUN, &addr);
    retE((ret < 0));
    return 0;
}

struct vstun_ops stun_core_ops = {
    .encode_msg  = _vstun_msg_enc,
    .decode_msg  = _vstun_msg_dec,
    .proc_msg    = _vstun_msg_proc,
    .render_srvc = _vstun_render_srvc
};

static
int _aux_stun_msg_cb(void* cookie, struct vmsg_usr* mu)
{
    struct vstun* stun = (struct vstun*)cookie;
    void* buf = NULL;
    int ret = 0;
    vassert(stun);
    vassert(mu);

    vattr_addrv4_init(&stun->from, to_sockaddr_sin(mu->addr));
    ret = stun->ops->decode_msg(stun, mu->data, mu->len);
    retE((ret < 0));
    ret = stun->ops->proc_msg(stun);
    retE((ret < 0));

    buf = malloc(BUF_SZ);
    vlog((!buf), elog_malloc);
    retE((!buf));
    ret = stun->ops->encode_msg(stun, buf, BUF_SZ);
    ret1E((ret < 0), free(buf));
    {
        struct vmsg_usr msg = {
            .addr  = mu->addr,
            .msgId = VMSG_STUN,
            .data  = buf,
            .len   = ret
        };
        ret = stun->msger.ops->push(&stun->msger, &msg);
        ret1E((ret < 0), free(buf));
    }
    return 0;
}

static
int _aux_stun_pack_msg_cb(void* cookie, struct vmsg_usr* um, struct vmsg_sys* sm)
{
    vassert(cookie);
    retE((um->msgId != VMSG_STUN));

    vmsg_sys_init(sm, um->addr, um->len, um->data);
    return 0;
}

static
int _aux_stun_unpack_msg_cb(void* cookie, struct vmsg_sys* sm, struct vmsg_usr* um)
{
    vassert(cookie);
    vassert(sm);
    vassert(um);

    vmsg_usr_init(um, VMSG_STUN, &sm->addr, sm->len, sm->data);
    return 0;
}

static
int _aux_stun_get_addr(struct vconfig* cfg, struct sockaddr_in* addr)
{
    char ip[64];
    int port = 0;
    int ret  = 0;
    vassert(cfg);

    memset(ip, 0, 64);
    ret = vhostaddr_get_first(ip, 64);
    vlog((ret < 0), elog_vhostaddr_get_first);
    retE((ret < 0));
    ret = cfg->inst_ops->get_stun_port(cfg, &port);
    retE((ret < 0));
    ret = vsockaddr_convert(ip, port, addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));
    return 0;
}

int vstun_init(struct vstun* stun, struct vhost* host, struct vconfig* cfg)
{
    struct sockaddr_in addr;
    int ret = 0;

    vassert(cfg);
    memset(stun, 0, sizeof(*stun));
    ret = _aux_stun_get_addr(cfg, &addr);
    retE((ret < 0));

    stun->has_source_addr = 1;
    vattr_addrv4_init(&stun->source, &addr);

    stun->has_alt_addr    = 0;
    stun->has_second_addr = 0;
    stun->host = host;
    stun->ops  = &stun_core_ops;

    ret += vmsger_init(&stun->msger);
    ret += vrpc_init(&stun->rpc, &stun->msger, VRPC_UDP, to_vsockaddr_from_sin(&addr));
    if (ret < 0) {
        vrpc_deinit(&stun->rpc);
        vmsger_deinit(&stun->msger);
        return -1;
    }
    vmsger_reg_pack_cb  (&stun->msger, _aux_stun_pack_msg_cb,   stun);
    vmsger_reg_unpack_cb(&stun->msger, _aux_stun_unpack_msg_cb, stun);
    stun->msger.ops->add_cb(&stun->msger, stun, _aux_stun_msg_cb, VMSG_STUN);

    return 0;
}

void vstun_deinit(struct vstun* stun)
{
    vassert(stun);

    vrpc_deinit   (&stun->rpc);
    vmsger_deinit (&stun->msger);

    return ;
}

