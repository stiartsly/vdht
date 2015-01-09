#include "vglobal.h"
#include "vnodeId.h"

static uint32_t token_dynamic_magic = 0;

static
uint32_t get_macaddr_hash(void)
{
    static uint8_t  macaddr[8];
    static uint32_t hash = 0;
    int got = 0;
    int ret = 0;
    int i = 0;

    if (!got) {
        memset(macaddr, 0, sizeof(macaddr));
        ret = vmacaddr_get(macaddr, 8);
        vlog((ret < 0), elog_vmacaddr_get);
        for (i = 0; i < 6; i++) {
            hash += (uint32_t)(macaddr[i] << i);
        }
        got = 1;
    }
    return hash;
}

/*
 * for vtoken funcs
 */
void vtoken_make(vtoken* token)
{
    uint32_t seed = (uint32_t)time(NULL);
    char* data = NULL;
    int i = 0;
    vassert(token);

    seed += token_dynamic_magic;
    seed += (uint32_t)pthread_self();
    seed += (uint32_t)getpid();
    seed += get_macaddr_hash();
    srand(seed);
    token_dynamic_magic = seed;

    data = (char*)token->data;
    for (; i < VTOKEN_LEN; i++) {
        data[i] = (uint8_t)rand();
    }
    return ;
}

int vtoken_equal(vtoken* a, vtoken* b)
{
    int i = 0;
    vassert(a);
    vassert(b);

    for (; i < VTOKEN_LEN; i++) {
        if (a->data[i] != b->data[i]) {
            return 0;
        }
    }
    return 1;
}

void vtoken_copy(vtoken* dst, vtoken* src)
{
    int i = 0;
    vassert(dst);
    vassert(src);

    for (; i < VTOKEN_LEN; i++) {
        dst->data[i] = src->data[i];
    }
    return ;
}

int vtoken_strlize(vtoken* token, char* buf, int len)
{
    uint8_t data = 0;
    int ret = 0;
    int sz  = 0;
    int i   = 0;

    vassert(token);
    vassert(buf);
    vassert(len > 0);

    for (; i < VTOKEN_LEN; i++) {
        data = token->data[i];
        ret = snprintf(buf+sz, len-sz, "%x%x", (data >> 4), (data & 0x0f));
        vlog((ret >= len-sz), elog_snprintf);
        retE((ret >= len-sz));
        sz += ret;
    }

    return 0;
}

int vtoken_unstrlize(const char* id_str, vtoken* token)
{
    uint32_t low  = 0;
    uint32_t high = 0;
    char data[2] = {'\0', '\0'};
    int ret = 0;
    int i = 0;
    vassert(id_str);
    vassert(token);

    for (; i < VTOKEN_LEN; i++) {
        data[0] = *id_str++;
        ret = sscanf(data, "%x", &high);
        vlog((!ret), elog_sscanf);
        retE((!ret));

        data[0] = *id_str++;
        ret = sscanf(data, "%x", &low);
        vlog((!ret), elog_sscanf);
        retE((!ret));

        token->data[i] = ((high << 4) | (0x0f & low));
    }
    return 0;
}

void vtoken_dump(vtoken* token)
{
    uint8_t data = 0;
    char buf[64];
    int i   = 0;
    int sz  = 0;
    int ret = 0;
    vassert(token);

    memset(buf, 0, 64);
    sz = snprintf(buf, 64, "ID:");

    for (; i < VTOKEN_LEN-1; i++) {
        data = token->data[i];
        ret = snprintf(buf+sz, 64-sz, "%x%x-", (data >> 4), (data & 0x0f));
        sz += ret;
    }
    data = token->data[i];
    snprintf(buf+sz, 64-sz, "%x%x", (data >> 4), (data & 0x0f));
    vdump(printf("%s",buf));

    return;
}

/*
 * for vnodeId funcs
 */
void vnodeId_dist(vnodeId* a, vnodeId* b, vnodeMetric* m)
{
    int i = 0;
    vassert(a);
    vassert(b);
    vassert(m);

    unsigned char* a_data = a->data;
    unsigned char* b_data = b->data;
    unsigned char* m_data = m->data;

    for (; i < VTOKEN_LEN; i++) {
        *(m_data++) = *(a_data++) ^ *(b_data++);
    }
    return ;
}

static
int _aux_distance(vnodeMetric* m)
{
    int i = 0;
    vassert(m);

    for (; i < VTOKEN_BITLEN; i++) {
        int bit  = VTOKEN_BITLEN - i - 1;
        int byte = i / 8;
        int bbit = 7 - (i % 8);
        unsigned char comp = ( 1 << bbit);
        if (comp & m->data[byte]) {
            return bit;
        }
    }
    return 0;
}

int vnodeId_bucket(vnodeId* a, vnodeId* b)
{
    vnodeMetric m;
    vassert(a);
    vassert(b);

    vnodeId_dist(a, b, &m);
    return _aux_distance(&m);
}

int vnodeMetric_cmp(vnodeMetric* a, vnodeMetric* b)
{
    unsigned char* a_data = a->data;
    unsigned char* b_data = b->data;
    int i = 0;

    vassert(a);
    vassert(b);

    for(; i < VTOKEN_LEN; i++){
        if (*a_data < *b_data) {
            return 1;
        } else if (*a_data > *b_data) {
            return -1;
        }
        a_data++;
        b_data++;
    }
    return 0;
}

/*
 * for vnodeVer funcs
 */
vtoken zero_node_ver = {
    .data = {0, 0, 0, 0, 0}
};

int vnodeVer_strlize(vnodeVer* ver, char* buf, int len)
{
    int offset = 0;
    int ret = 0;
    int i = 0;
    vassert(ver);
    vassert(buf);
    vassert(len > 0);

    ret = snprintf(buf, len - ret, "%d", ver->data[i]);
    retE((ret < 0));
    offset += ret;
    i++;

    for (; i < VTOKEN_INTLEN; i++) {
        ret = snprintf(buf + offset, len - offset, ".");
        vlog((ret >= len-offset), elog_snprintf);
        retE((ret >= len-offset));
        offset += ret;

        ret = snprintf(buf + offset, len - offset, "%d", ver->data[i]);
        vlog((ret >= len-offset), elog_snprintf);
        retE((ret >= len-offset));
        offset += ret;
    }
    return 0;
}

int vnodeVer_unstrlize(const char* ver_str, vnodeVer* ver)
{
    char* s = (char*)ver_str;
    int32_t ret = 0;
    int i = 0;
    vassert(ver_str);
    vassert(ver);

    errno = 0;
    ret = strtol(s, &s, 10);
    vlog((errno), elog_strtol);
    retE((errno));
    ver->data[i] = ret;
    i++;

    while(*s != '\0') {
        retE((*s != '.'));
        s++;

        errno = 0;
        ret = strtol(s, &s, 10);
        vlog((errno), elog_strtol);
        retE((errno));
        ver->data[i] = ret;
        i++;
    }
    return 0;
}

void vnodeVer_dump(vnodeVer* ver)
{
    char sver[64];
    vassert(ver);

    memset(sver, 0, 64);
    vnodeVer_strlize(ver, sver, 64);
    if (!strcmp("0.0.0.0.0", sver)) {
        vdump(printf("empty"));
    } else {
        vdump(printf("ver:%s", sver));
    }
    return;
}

/*
 * for vnodeInfo funcs
 */
static MEM_AUX_INIT(node_info_cache, sizeof(vnodeInfo), 0);
vnodeInfo* vnodeInfo_alloc(void)
{
    vnodeInfo* info = NULL;

    info = (vnodeInfo*)vmem_aux_alloc(&node_info_cache);
    vlog((!info), elog_vmem_aux_alloc);
    retE_p((!info));
    memset(info, 0, sizeof(*info));
    return info;
}

void vnodeInfo_free(vnodeInfo* info)
{
    vmem_aux_free(&node_info_cache, info);
    return ;
}

int vnodeInfo_equal(vnodeInfo* a, vnodeInfo* b)
{
    vassert(a);
    vassert(b);

    if (vtoken_equal(&a->id, &b->id)) {
        return 1;
    }
    return (vsockaddr_equal(&a->laddr, &b->laddr)
         && vsockaddr_equal(&a->uaddr, &b->uaddr)
         && vsockaddr_equal(&a->eaddr, &b->eaddr));
}

void vnodeInfo_copy(vnodeInfo* dest, vnodeInfo* src)
{
    vassert(dest);
    vassert(src);

    vtoken_copy(&dest->id,  &src->id);
    vtoken_copy(&dest->ver, &src->ver);
    vsockaddr_copy(&dest->laddr, &src->laddr);
    vsockaddr_copy(&dest->uaddr, &src->uaddr);
    vsockaddr_copy(&dest->eaddr, &src->eaddr);
    vsockaddr_copy(&dest->raddr, &src->raddr);
    dest->weight = src->weight;
    return ;
}

int vnodeInfo_init(vnodeInfo* info, vnodeId* id, struct sockaddr_in* addr, vnodeVer* ver, int32_t weight)
{
    vassert(info);
    vassert(id);
    vassert(addr);
    vassert(ver);
    vassert(weight >= 0);

    vtoken_copy(&info->id,  id );
    vtoken_copy(&info->ver, ver);
    vsockaddr_copy(&info->laddr, addr);
    vsockaddr_copy(&info->uaddr, addr);
    vsockaddr_copy(&info->eaddr, addr);
    vsockaddr_copy(&info->raddr, addr);
    info->weight = weight;
    return 0;
}

void vnodeInfo_dump(vnodeInfo* info)
{
    vassert(info);

    vtoken_dump(&info->id);
    vnodeVer_dump(&info->ver);
    vsockaddr_dump(&info->laddr);
    vsockaddr_dump(&info->uaddr);
    vsockaddr_dump(&info->eaddr);
    vdump(printf("weight: %d", info->weight));
    return ;
}

int vnodeInfo_has_addr(vnodeInfo* info, struct sockaddr_in* addr)
{
    vassert(info);
    vassert(addr);

    return (vsockaddr_equal(&info->laddr, addr)
         || vsockaddr_equal(&info->uaddr, addr)
         || vsockaddr_equal(&info->eaddr, addr));
}

/*
 * for vsrvcId
 */
int vsrvcId_bucket(vsrvcId* id)
{
    int hash = 0;
    int i = 0;
    vassert(id);

    for (i = 0; i < VTOKEN_LEN; i++) {
        hash += (id->data[i]) % VTOKEN_BITLEN;
    }
    return (hash % VTOKEN_BITLEN);
}

/*
 * for vsrvcInfo funcs
 */
static MEM_AUX_INIT(srvc_info_cache, sizeof(vsrvcInfo), 0);
vsrvcInfo* vsrvcInfo_alloc(void)
{
    vsrvcInfo* info = NULL;

    info = (vsrvcInfo*)vmem_aux_alloc(&srvc_info_cache);
    vlog((!info), elog_vmem_aux_alloc);
    retE_p((!info));
    memset(info, 0, sizeof(*info));
    return info;
}

void vsrvcInfo_free(vsrvcInfo* svc_info)
{
    vmem_aux_free(&srvc_info_cache, svc_info);
    return ;
}

int vsrvcInfo_init(vsrvcInfo* svc_info, vtoken* svc_id, int32_t nice, struct sockaddr_in* addr)
{
    vassert(svc_info);
    vassert(addr);

    vtoken_copy(&svc_info->id, svc_id);
    svc_info->nice = nice;
    vsockaddr_copy(&svc_info->addr, addr);
    return 0;
}

void vsrvcInfo_copy (vsrvcInfo* dest, vsrvcInfo* src)
{
    vassert(dest);
    vassert(src);

    vtoken_copy(&dest->id,  &src->id);
    vsockaddr_copy(&dest->addr, &src->addr);
    dest->nice = src->nice;

    return ;
}

int vsrvcInfo_equal(vsrvcInfo* a, vsrvcInfo* b)
{
    vassert(a);
    vassert(b);

    return (vtoken_equal(&a->id, &b->id) && vsockaddr_equal(&a->addr,&b->addr));
}

void vsrvcInfo_dump(vsrvcInfo* svc_info)
{
    vassert(svc_info);
    vtoken_dump(&svc_info->id);
    vsockaddr_dump(&svc_info->addr);
    vdump(printf("nice:%d", svc_info->nice));

    return ;
}

