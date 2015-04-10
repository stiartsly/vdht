#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "vlsctlc.h"

#define vassert(exp) do { \
	if (!exp) {\
            printf("{assert} [%s:%d]\n", __FUNCTION__, __LINE__);\
	    *(int*)0 = 0; \
        } \
    } while(0)

static
int _vlsctlc_bind_cmd_host_up(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_down ||
        lsctlc->bind_host_exit) {
        return -1;
    }
    lsctlc->bind_host_up = 1;
    return 0;
}

static
int _vlsctlc_bind_cmd_host_down(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);


    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_up ||
        lsctlc->bind_host_exit) {
        return -1;
    }
    lsctlc->bind_host_down = 1;
    return 0;
}

static
int _vlsctlc_bind_cmd_host_exit(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_up ||
        lsctlc->bind_host_exit ||
        lsctlc->bind_host_dump ||
        lsctlc->bind_cfg_dump ||
        lsctlc->bind_bogus_query ||
        lsctlc->bind_post_service ||
        lsctlc->bind_unpost_service) {
        return -1;
    }

    lsctlc->bind_host_exit = 1;
    return 0;
}

static
int _vlsctlc_bind_cmd_host_dump(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_exit) {
        return -1;
    }
    lsctlc->bind_host_dump= 1;
    return 0;
}

static
int _vlsctlc_bind_cmd_cfg_dump(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_exit) {
        return -1;
    }
    lsctlc->bind_cfg_dump = 1;
    return 0;
}

static
int _vlsctlc_bind_cmd_join_node(struct vlsctlc* lsctlc, struct sockaddr_in* addr)
{
    vassert(lsctlc);
    vassert(addr);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_exit ||
        lsctlc->bind_join_node) {
        return -1;
    }
    lsctlc->bind_join_node = 1;
    memcpy(&lsctlc->addr1, addr, sizeof(*addr));
    return 0;
}

static
int _vlsctlc_bind_cmd_bogus_query(struct vlsctlc* lsctlc, int queryId, struct sockaddr_in* addr)
{
    vassert(lsctlc);
    vassert(queryId);
    vassert(addr);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_exit ||
        lsctlc->bind_bogus_query) {
        return -1;
    }
    lsctlc->bind_bogus_query = 1;
    lsctlc->queryId = queryId;
    memcpy(&lsctlc->addr2, addr, sizeof(*addr));
    return 0;
}

static
int _vlsctlc_bind_cmd_post_service(struct vlsctlc* lsctlc, vsrvcHash* hash, struct sockaddr_in* addr)
{
    vassert(lsctlc);
    vassert(hash);
    vassert(addr);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_exit ||
        lsctlc->bind_post_service ||
        lsctlc->bind_unpost_service) {
        return -1;
    }
    lsctlc->bind_post_service = 1;
    memcpy(lsctlc->hash1.data, hash->data, VTOKEN_LEN);
    memcpy(&lsctlc->addr3, addr, sizeof(*addr));
    return 0;
}

static
int _vlsctlc_bind_cmd_unpost_service(struct vlsctlc* lsctlc, vsrvcHash* hash, struct sockaddr_in* addr)
{
    vassert(lsctlc);
    vassert(hash);
    vassert(addr);

    switch(lsctlc->type) {
    case vlsctl_cmd:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_cmd;
        break;
    default:
        return -1;
    }

    if (lsctlc->bind_host_exit ||
        lsctlc->bind_post_service ||
        lsctlc->bind_unpost_service) {
        return -1;
    }
    lsctlc->bind_unpost_service = 1;
    memcpy(lsctlc->hash2.data, hash->data, VTOKEN_LEN);
    memcpy(&lsctlc->addr4, addr, sizeof(*addr));
    return 0;
}

static
int _vlsctlc_bind_cmd_probe_service(struct vlsctlc* lsctlc, vsrvcHash* hash)
{
    vassert(lsctlc);
    vassert(hash);

    switch(lsctlc->type) {
    case vlsctl_req:
        break;
    case vlsctl_raw:
        lsctlc->type = vlsctl_req;
        break;
    default:
        return -1;
    }
    lsctlc->bind_probe_service = 1;
    memcpy(lsctlc->hash3.data, hash->data, VTOKEN_LEN);
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
    .bind_probe_service  = _vlsctlc_bind_cmd_probe_service
};

static
int _aux_vlsctlc_pack_addr(void* buf, int len, struct sockaddr_in* addr)
{
    int tsz = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    *(int16_t*)(buf + tsz) = (int16_t)addr->sin_family;
    tsz += sizeof(int16_t);
    *(int16_t*)(buf + tsz) = (int16_t)addr->sin_port;
    tsz += sizeof(int16_t);
    *(uint32_t*)(buf + tsz) = (uint32_t)addr->sin_addr.s_addr;
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

    if (!lsctlc->bind_host_up) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_HOST_UP;
    tsz += sizeof(uint16_t);

    *(uint16_t*)(buf + tsz) = 0;
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_host_down(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_host_down) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_HOST_DOWN;
    tsz += sizeof(uint16_t);

    *(uint16_t*)(buf + tsz) = 0;
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_host_exit(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_host_exit) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_HOST_EXIT;
    tsz += sizeof(uint16_t);

    *(uint16_t*)(buf + tsz) = 0;
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_host_dump(struct vlsctlc* lsctlc, void* buf, int len)
{
   int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_host_dump) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_HOST_DUMP;
    tsz += sizeof(uint16_t);

    *(uint16_t*)(buf + tsz) = 0;
    tsz += sizeof(uint16_t);

    return tsz;
}

int _vlsctlc_pack_cmd_cfg_dump(struct vlsctlc* lsctlc, void* buf, int len)
{
    int tsz = 0;
    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_cfg_dump) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_CFG_DUMP;
    tsz += sizeof(uint16_t);

    *(uint16_t*)(buf + tsz) = 0;
    tsz += sizeof(uint16_t);

    return tsz;
}

static
int _vlsctlc_pack_cmd_join_node(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct sockaddr_in* addr = &lsctlc->addr1;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_join_node) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_JOIN_NODE;
    tsz += sizeof(uint16_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    bsz = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, addr);
    tsz += bsz;

    *(uint16_t*)len_addr = (uint16_t)bsz;
    return tsz;
}

static
int _vlsctlc_pack_cmd_bogus_query(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct sockaddr_in* addr = &lsctlc->addr2;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_bogus_query) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_BOGUS_QUERY;
    tsz += sizeof(uint16_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    *(int32_t*)(buf + tsz) = (uint32_t)lsctlc->queryId;
    bsz += sizeof(int32_t);
    tsz += sizeof(int32_t);

    ret = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, addr);
    bsz += ret;
    tsz += ret;

    *(uint16_t*)len_addr = (uint16_t)bsz;
    return tsz;
}

static
int _vlsctlc_pack_cmd_post_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct sockaddr_in* addr = &lsctlc->addr3;
    vsrvcHash* hash = &lsctlc->hash1;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_post_service) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_POST_SERVICE;
    tsz += sizeof(uint16_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &hash->data, VTOKEN_LEN);
    bsz += VTOKEN_LEN;
    tsz += VTOKEN_LEN;

    ret = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, addr);
    bsz += ret;
    tsz += ret;

    *(uint16_t*)len_addr = (uint16_t)bsz;
    return tsz;
}

static
int _vlsctlc_pack_cmd_unpost_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct sockaddr_in* addr = &lsctlc->addr4;
    vsrvcHash* hash = &lsctlc->hash2;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;
    int ret = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_unpost_service) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_UNPOST_SERVICE;
    tsz += sizeof(uint16_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &hash->data, VTOKEN_LEN);
    bsz += VTOKEN_LEN;
    tsz += VTOKEN_LEN;

    ret = _aux_vlsctlc_pack_addr(buf + tsz, len - tsz, addr);
    bsz += ret;
    tsz += ret;

    *(uint16_t*)len_addr = (uint16_t)bsz;
    return tsz;
}

static
int _vlsctlc_pack_cmd_probe_service(struct vlsctlc* lsctlc, void* buf, int len)
{
    vsrvcHash* hash = &lsctlc->hash3;
    void* len_addr = NULL;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    if (!lsctlc->bind_probe_service) {
        return 0;
    }
    *(uint16_t*)(buf + tsz) = VLSCTL_PROBE_SERVICE;
    tsz += sizeof(uint16_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    memcpy(buf + tsz, &hash->data, VTOKEN_LEN);
    bsz += VTOKEN_LEN;
    tsz += VTOKEN_LEN;

    *(uint16_t*)len_addr = (uint16_t)bsz;
    return tsz;
}

static
struct vlsctlc_pack_cmd_desc lsctlc_cmds_desc[] = {
    {"host up",        VLSCTL_HOST_UP,        _vlsctlc_pack_cmd_host_up        },
    {"host down",      VLSCTL_HOST_DOWN,      _vlsctlc_pack_cmd_host_down      },
    {"host exit",      VLSCTL_HOST_EXIT,      _vlsctlc_pack_cmd_host_exit      },
    {"dump host",      VLSCTL_HOST_DUMP,      _vlsctlc_pack_cmd_host_dump      },
    {"dump cfg",       VLSCTL_CFG_DUMP,       _vlsctlc_pack_cmd_cfg_dump       },
    {"join node",      VLSCTL_JOIN_NODE,      _vlsctlc_pack_cmd_join_node      },
    {"bogus query",    VLSCTL_BOGUS_QUERY,    _vlsctlc_pack_cmd_bogus_query    },
    {"post service",   VLSCTL_POST_SERVICE,   _vlsctlc_pack_cmd_post_service   },
    {"unpost service", VLSCTL_UNPOST_SERVICE, _vlsctlc_pack_cmd_unpost_service },
    {"probe service",  VLSCTL_PROBE_SERVICE,  _vlsctlc_pack_cmd_probe_service  },
    {NULL, VLSCTL_BUTT, NULL }
};

static
int _vlsctlc_pack_cmds(struct vlsctlc* lsctlc, void* buf, int len)
{
    struct vlsctlc_pack_cmd_desc* desc = lsctlc_cmds_desc;
    void* len_addr = NULL;
    int ret = 0;
    int tsz = 0;
    int bsz = 0;

    vassert(lsctlc);
    vassert(buf);
    vassert(len > 0);

    *(uint8_t*)(buf + tsz) = VLSCTL_VERSION;
    tsz += sizeof(uint8_t);

    *(uint8_t*)(buf + tsz) = (uint8_t)lsctlc->type;
    tsz += sizeof(uint8_t);

    len_addr = buf + tsz;
    *(uint16_t*)len_addr = 0;
    tsz += sizeof(uint16_t);

    *(uint32_t*)(buf + tsz) = VLSCTL_MAGIC;
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

struct vlsctlc_ops lsctlc_ops = {
    .pack_cmds = _vlsctlc_pack_cmds
};


int vlsctlc_init(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    memset(lsctlc, 0, sizeof(*lsctlc));
    lsctlc->type = vlsctl_raw;
    lsctlc->ops  = &lsctlc_ops;
    lsctlc->bind_ops = &lsctlc_bind_ops;

    return 0;
}

void vlsctlc_deinit(struct vlsctlc* lsctlc)
{
    vassert(lsctlc);

    //do nothing;
    return ;
}

