#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "vlsctlc.h"

static
void _aux_vhexbuf_dump(uint8_t* hex_buf, int len)
{
    int i = 0;

    vassert(hex_buf);
    vassert(len > 0);

    for (i = 0; i < len ; i++) {
        if (i % 16 == 0) {
            printf("0x%p: ", (void*)(hex_buf + i));
        }
        printf("%02x ", hex_buf[i]);
        if (i % 16 == 15) {
            printf("\n");
        }
    }
    printf("\n");
    return ;
}

static
int _vlsctlc_bind_cmd_host_up(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_HOST_UP;
    lsctlc->type = vlsctl_cmd;

    return 0;
}

static
int _vlsctlc_bind_cmd_host_down(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_HOST_DOWN;
    lsctlc->type = vlsctl_cmd;

    return 0;
}

static
int _vlsctlc_bind_cmd_host_exit(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_HOST_EXIT;
    lsctlc->type = vlsctl_cmd;

    return 0;
}

static
int _vlsctlc_bind_cmd_host_dump(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_HOST_DUMP;
    lsctlc->type = vlsctl_cmd;

    return 0;
}

static
int _vlsctlc_bind_cmd_cfg_dump(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_CFG_DUMP;
    lsctlc->type = vlsctl_cmd;

    return 0;
}

static
int _vlsctlc_bind_cmd_join_node(struct vlsctlc* lsctlc, struct sockaddr_in* addr)
{
    vassert(lsctlc);
    vassert(addr);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_JOIN_NODE;
    lsctlc->type = vlsctl_cmd;
    memcpy(&lsctlc->args.join_node_args.addr, addr, sizeof(*addr));

    return 0;
}

static
int _vlsctlc_bind_cmd_bogus_query(struct vlsctlc* lsctlc, int queryId, struct sockaddr_in* addr)
{
    struct bogus_query_args* args = &lsctlc->args.bogus_query_args;

    vassert(lsctlc);
    //vassert(queryId >= 0);
    vassert(addr);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }

    lsctlc->bound_cmd = VLSCTL_BOGUS_QUERY;
    lsctlc->type = vlsctl_cmd;
    args->queryId = (int32_t)queryId;
    memcpy(&args->addr, addr, sizeof(struct sockaddr_in));
    return 0;
}

static
int _vlsctlc_bind_cmd_post_service(struct vlsctlc* lsctlc, vsrvcHash* hash, struct vsockaddr_in* addrs, int num, int proto)
{
    struct post_service_args* args = &lsctlc->args.post_service_args;
    int i = 0;

    vassert(lsctlc);
    vassert(hash);
    vassert(addrs);
    vassert(num > 0);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }
    lsctlc->bound_cmd = VLSCTL_POST_SERVICE;
    lsctlc->type = vlsctl_cmd;

    args->proto  = proto;
    args->naddrs = num;
    memcpy(&args->hash, hash, sizeof(vsrvcHash));
    for (i = 0; i < num; i++) {
        memcpy(&args->addrs[i], &addrs[i], sizeof(struct vsockaddr_in));
    }
    return 0;
}

static
int _vlsctlc_bind_cmd_unpost_service(struct vlsctlc* lsctlc, vsrvcHash* hash, struct sockaddr_in* addrs, int num)
{
    struct unpost_service_args* args = &lsctlc->args.unpost_service_args;

    int i = 0;
    vassert(lsctlc);
    vassert(hash);
    vassert(addrs);
    //vassert(num >= 0);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }
    lsctlc->bound_cmd = VLSCTL_UNPOST_SERVICE;
    lsctlc->type = vlsctl_cmd;

    memcpy(&args->hash, hash, sizeof(vsrvcHash));
    for (i = 0; i < num; i++) {
        memcpy(&args->addrs[i], &addrs[i], sizeof(struct sockaddr_in));
    }
    args->naddrs = num;
    return 0;
}

static
int _vlsctlc_bind_cmd_find_service(struct vlsctlc* lsctlc, vsrvcHash* hash)
{
    struct find_service_args* args = &lsctlc->args.find_service_args;
    vassert(lsctlc);
    vassert(hash);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }
    lsctlc->bound_cmd = VLSCTL_FIND_SERVICE;
    lsctlc->type = vlsctl_req;

    memcpy(&args->hash, hash, sizeof(vsrvcHash));
    return 0;
}

static
int _vlsctlc_bind_cmd_probe_service(struct vlsctlc* lsctlc, vsrvcHash* hash)
{
    struct probe_service_args* args = &lsctlc->args.probe_service_args;
    vassert(lsctlc);
    vassert(hash);

    if (lsctlc->bound_cmd > 0) {
        return -1;
    }
    lsctlc->bound_cmd = VLSCTL_PROBE_SERVICE;
    lsctlc->type = vlsctl_req;

    memcpy(&args->hash, hash, sizeof(vsrvcHash));
    return 0;
}

static
struct vlsctlc_bind_ops lsctlc_bind_ops = {
    .bind_host_up        = _vlsctlc_bind_cmd_host_up,
    .bind_host_down      = _vlsctlc_bind_cmd_host_down,
    .bind_host_exit      = _vlsctlc_bind_cmd_host_exit,
    .bind_host_dump      = _vlsctlc_bind_cmd_host_dump,
    .bind_cfg_dump       = _vlsctlc_bind_cmd_cfg_dump,
    .bind_join_node      = _vlsctlc_bind_cmd_join_node,
    .bind_bogus_query    = _vlsctlc_bind_cmd_bogus_query,
    .bind_post_service   = _vlsctlc_bind_cmd_post_service,
    .bind_unpost_service = _vlsctlc_bind_cmd_unpost_service,
    .bind_find_service   = _vlsctlc_bind_cmd_find_service,
    .bind_probe_service  = _vlsctlc_bind_cmd_probe_service
};

static
int _vlsctlc_unbind_cmd_find_service(struct vlsctlc* lsctlc, vsrvcHash* hash, struct vsockaddr_in* addrs, int* num, int* proto)
{
    struct find_service_rsp_args* args = &lsctlc->rsp_args.find_service_rsp_args;
    vassert(lsctlc);
    vassert(hash);
    vassert(addrs);
    vassert(num);
    vassert(proto);
    int i = 0;

    if (lsctlc->bound_cmd != VLSCTL_FIND_SERVICE) {
        return -1;
    }
    if (lsctlc->type == vlsctl_rsp_err) {
        //todo with errno.
        return -1;
    }

    memcpy(hash, &args->hash, sizeof(vsrvcHash));
    *proto = args->proto;
    *num   = args->num;
    for (i = 0; i < args->num; i++) {
        memcpy(&addrs[i], &args->addrs[i], sizeof(struct vsockaddr_in));
    }
    return 0;
}

static
int _vlsctlc_unbind_cmd_probe_service(struct vlsctlc* lsctlc, vsrvcHash* hash, struct vsockaddr_in* addrs, int* num, int* proto)
{
    struct probe_service_rsp_args* args = &lsctlc->rsp_args.probe_service_rsp_args;
    vassert(lsctlc);
    vassert(hash);
    vassert(addrs);
    vassert(num);
    vassert(proto);
    int i = 0;

    if (lsctlc->bound_cmd != VLSCTL_PROBE_SERVICE) {
        return -1;
    }
    if (lsctlc->type == vlsctl_rsp_err) {
        //todo with errno.
        return -1;
    }

    memcpy(hash, &args->hash, sizeof(vsrvcHash));
    *proto = args->proto;
    *num   = args->num;
    for (i = 0; i < args->num; i++) {
        memcpy(&addrs[i], &args->addrs[i], sizeof(struct vsockaddr_in));
    }
    return 0;
}

static
struct vlsctlc_unbind_ops lsctlc_unbind_ops = {
    .unbind_find_service  = _vlsctlc_unbind_cmd_find_service,
    .unbind_probe_service = _vlsctlc_unbind_cmd_probe_service,
};

static
int _aux_vlsctlc_pack_addr(void* buf, int len, struct sockaddr_in* addr)
{
    int tsz = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    tsz += sizeof(uint16_t); // skip family;
    set_uint16(buf + tsz, addr->sin_port);
    tsz += sizeof(int16_t);
    set_uint32(buf + tsz, addr->sin_addr.s_addr);
    tsz += sizeof(uint32_t);

    return tsz;
}

static
int _aux_vlsctlc_pack_vaddr(void* buf, int len, struct vsockaddr_in* addr)
{
    int tsz = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    tsz += sizeof(uint16_t); // skip family;
    set_uint16(buf + tsz, addr->addr.sin_port);
    tsz += sizeof(int16_t);
    set_uint32(buf + tsz, addr->addr.sin_addr.s_addr);
    tsz += sizeof(uint32_t);
    set_uint32(buf + tsz, addr->type);
    tsz += sizeof(uint32_t);

    return tsz;
}

static
int _vlsctlc_pack_cmd_host_up(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_HOST_UP) {
        return 0;
    }

    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(buf + tsz, 0);
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_host_down(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_HOST_DOWN) {
        return 0;
    }

    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(buf + tsz, 0);
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_host_exit(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_HOST_EXIT) {
        return 0;
    }

    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(buf + tsz, 0);
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_host_dump(struct vlsctlc* lsctlc, void* buf, int len)
{
   int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_HOST_DUMP) {
        return 0;
    }

    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(buf + tsz, 0);
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_cfg_dump(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_CFG_DUMP) {
        return 0;
    }

    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(buf + tsz, 0);
    tsz += sizeof(uint16_t);

    return tsz;
}

static
int _vlsctlc_pack_cmd_join_node(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct join_node_args* args = &lsctlc->args.join_node_args;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_JOIN_NODE) {
        return 0;
    }
    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);

    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    bsz = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, &args->addr);
    tsz += bsz;

    set_uint16(len_addr, bsz);
    return tsz;
}

static
int _vlsctlc_pack_cmd_bogus_query(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct bogus_query_args* args = &lsctlc->args.bogus_query_args;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_BOGUS_QUERY) {
        return 0;
    }
    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    set_int32(buf + tsz, args->queryId);
    bsz += sizeof(int32_t);
    tsz += sizeof(int32_t);

    ret = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, &args->addr);
    bsz += ret;
    tsz += ret;

    set_uint16(len_addr, bsz);
    return tsz;
}

static
int _vlsctlc_pack_cmd_post_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct post_service_args* args = &lsctlc->args.post_service_args;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;
    int i = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_POST_SERVICE) {
        return 0;
    }
    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);

    set_int32(buf + tsz, args->proto);
    bsz += sizeof(int32_t);
    tsz += sizeof(int32_t);

    for (i = 0; i < args->naddrs; i++) {
        ret = _aux_vlsctlc_pack_vaddr(buf + tsz, len - tsz, &args->addrs[i]);
        bsz += ret;
        tsz += ret;
    }
    set_uint16(len_addr, bsz);
    return tsz;
}

static
int _vlsctlc_pack_cmd_unpost_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct unpost_service_args* args = &lsctlc->args.unpost_service_args;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;
    int i = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_UNPOST_SERVICE) {
        return 0;
    }
    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);

    for (i = 0;i < args->naddrs; i++) {
        ret = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, &args->addrs[i]);
        bsz += ret;
        tsz += ret;
    }
    set_uint16(len_addr, bsz);
    return tsz;
}

static
int _vlsctlc_pack_cmd_find_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct find_service_args* args = &lsctlc->args.find_service_args;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_FIND_SERVICE) {
        return 0;
    }
    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);

    set_uint16(len_addr, bsz);
    return tsz;
}

static
int _vlsctlc_pack_cmd_probe_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct probe_service_args* args = &lsctlc->args.probe_service_args;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_PROBE_SERVICE) {
        return 0;
    }

    set_uint16(buf + tsz, lsctlc->bound_cmd);
    tsz += sizeof(uint16_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &args->hash, sizeof(vsrvcHash));
    bsz += sizeof(vsrvcHash);
    tsz += sizeof(vsrvcHash);

    set_uint16(len_addr, bsz);
    return tsz;
}

static
struct vlsctlc_pack_cmd_desc lsctlc_pack_cmd_desc[] = {
    {"host up",        VLSCTL_HOST_UP,        _vlsctlc_pack_cmd_host_up        },
    {"host down",      VLSCTL_HOST_DOWN,      _vlsctlc_pack_cmd_host_down      },
    {"host exit",      VLSCTL_HOST_EXIT,      _vlsctlc_pack_cmd_host_exit      },
    {"dump host",      VLSCTL_HOST_DUMP,      _vlsctlc_pack_cmd_host_dump      },
    {"dump cfg",       VLSCTL_CFG_DUMP,       _vlsctlc_pack_cmd_cfg_dump       },
    {"join node",      VLSCTL_JOIN_NODE,      _vlsctlc_pack_cmd_join_node      },
    {"bogus query",    VLSCTL_BOGUS_QUERY,    _vlsctlc_pack_cmd_bogus_query    },
    {"post service",   VLSCTL_POST_SERVICE,   _vlsctlc_pack_cmd_post_service   },
    {"unpost service", VLSCTL_UNPOST_SERVICE, _vlsctlc_pack_cmd_unpost_service },
    {"find service",   VLSCTL_FIND_SERVICE,   _vlsctlc_pack_cmd_find_service   },
    {"probe service",  VLSCTL_PROBE_SERVICE,  _vlsctlc_pack_cmd_probe_service  },
    {NULL, VLSCTL_BUTT, NULL }
};

static
int _aux_vlsctlc_unpack_vaddr(void* buf, int len, struct vsockaddr_in* addr)
{
    int tsz = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    memset(addr, 0, sizeof(*addr));

    tsz += sizeof(uint8_t);
    addr->addr.sin_family = get_uint8(buf + tsz);
    tsz += sizeof(uint8_t);
    addr->addr.sin_port = get_uint16(buf + tsz);
    tsz += sizeof(uint16_t);
    addr->addr.sin_addr.s_addr = get_uint32(buf + tsz);
    tsz += sizeof(uint32_t);
    addr->type = get_uint32(buf + tsz);
    tsz += sizeof(uint32_t);

    return tsz;
}


static
int _vlsctlc_unpack_cmd_find_service_rsp(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct find_service_rsp_args* args = &lsctlc->rsp_args.find_service_rsp_args;
    int tsz = 0;
    int i = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_FIND_SERVICE) {
        return 0;
    }

    memcpy(&args->hash, buf + tsz, sizeof(vsrvcHash));
    tsz += sizeof(vsrvcHash);
    args->proto = *(int32_t*)(buf + tsz);
    tsz += sizeof(int32_t);

    while(len - tsz > 0) {
        tsz += _aux_vlsctlc_unpack_vaddr(buf + tsz, len - tsz, &args->addrs[i]);
        i++;
    }
    args->num = i;
    return tsz;
}

static
int _vlsctlc_unpack_cmd_probe_service_rsp(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct probe_service_rsp_args* args = &lsctlc->rsp_args.probe_service_rsp_args;
    int tsz = 0;
    int i = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (lsctlc->bound_cmd != VLSCTL_PROBE_SERVICE) {
        return 0;
    }

    memcpy(&args->hash, buf + tsz, sizeof(vsrvcHash));
    tsz += sizeof(vsrvcHash);
    args->proto = *(int32_t*)(buf + tsz);
    tsz += sizeof(int32_t);

    while(len - tsz > 0) {
        tsz += _aux_vlsctlc_unpack_vaddr(buf + tsz, len - tsz, &args->addrs[i]);
        i++;
    }
    args->num = i;
    return tsz;
}

static
struct vlsctlc_unpack_cmd_desc lsctlc_unpack_cmd_desc[] = {
    {"find_service",   VLSCTL_FIND_SERVICE,   _vlsctlc_unpack_cmd_find_service_rsp },
    {"probe_service",  VLSCTL_PROBE_SERVICE,  _vlsctlc_unpack_cmd_probe_service_rsp},
    {NULL, VLSCTL_BUTT, NULL}
};

static
int _vlsctlc_pack_cmd(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct vlsctlc_pack_cmd_desc* desc = lsctlc_pack_cmd_desc;
    void* len_addr = NULL;
    int ret = 0;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    set_uint8(buf + tsz, VLSCTL_VERSION);
    tsz += sizeof(uint8_t);
    set_uint8(buf + tsz, (uint8_t)lsctlc->type);
    tsz += sizeof(uint8_t);
    set_uint16(len_addr = buf + tsz, 0);
    tsz += sizeof(uint16_t);
    set_uint32(buf + tsz, VLSCTL_MAGIC);
    tsz += sizeof(uint32_t);

    for (; desc->cmd; desc++) {
        ret = desc->cmd(lsctlc, buf + tsz, len - tsz);
        if (ret < 0) {
            return -1;
        }
        bsz += ret;
        tsz += ret;
    }
    *(uint16_t*)len_addr = (uint16_t)bsz;
    return tsz;
}

static
int _vlsctlc_unpack_cmd(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct vlsctlc_unpack_cmd_desc* desc = lsctlc_unpack_cmd_desc;
    uint32_t magic = 0;
    int tsz = 0;
    int bsz = 0;
    int csz = 0;
    int ret = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    _aux_vhexbuf_dump(buf, len);

    tsz += sizeof(uint8_t); // skip version;
    lsctlc->type = get_uint8(buf + tsz);
    tsz += sizeof(uint8_t);
    bsz  = (int)get_uint16(buf + tsz);
    tsz += sizeof(uint16_t);
    magic = get_uint32(buf + tsz);
    tsz += sizeof(uint32_t);

    if (magic != VLSCTL_MAGIC) {
        return -1;
    }

    while(bsz - csz > 0) {
        uint16_t cmd_id = 0;
        int cmd_len = 0;
        int i = 0;

        cmd_id = get_uint16(buf + tsz);
        tsz += sizeof(uint16_t);
        csz += sizeof(uint16_t);
        cmd_len = get_uint16(buf + tsz);
        tsz += sizeof(uint16_t);
        csz += sizeof(uint16_t);
        lsctlc->bound_cmd = cmd_id;
        while(cmd_len > 0) {
            for (i = 0; desc[i].cmd; i++) {
                ret = desc[i].cmd(lsctlc, buf + tsz, cmd_len);
                if (ret < 0) {
                    return -1;
                } else if (ret > 0) {
                    tsz += ret;
                    csz += ret;
                    cmd_len -= ret;
                    break;
                }else {
                    //do nothing;
                }
            }
        }
    }
    return 0;
}

struct vlsctlc_ops lsctlc_ops = {
    .pack_cmd   = _vlsctlc_pack_cmd,
    .unpack_cmd = _vlsctlc_unpack_cmd
};

int vlsctlc_init(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    memset(lsctlc, 0, sizeof(*lsctlc));
    lsctlc->type = vlsctl_raw;
    lsctlc->ops  = &lsctlc_ops;
    lsctlc->bind_ops = &lsctlc_bind_ops;
    lsctlc->unbind_ops = &lsctlc_unbind_ops;

    return 0;
}

void vlsctlc_deinit(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    //do nothing;
    return ;
}

