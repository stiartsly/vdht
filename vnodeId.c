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
    while(i < VTOKEN_LEN - 2) {
        data = token->data[i];
        ret = snprintf(buf+sz, 64-sz, "%x%x",  (data >> 4), (data & 0x0f));
        sz += ret;
        i++;

        data = token->data[i];
        ret = snprintf(buf+sz, 64-sz, "%x%x-", (data >> 4), (data & 0x0f));
        sz += ret;
        i++;
    }
    data = token->data[i];
    ret = snprintf(buf+sz, 64-sz, "%x%x", (data >> 4), (data & 0x0f));
    sz += ret;
    i++;

    data = token->data[i];
    ret = snprintf(buf+sz, 64-sz, "%x%x", (data >> 4), (data & 0x0f));
    sz += ret;
    i++;
    printf("%s", buf);
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
vnodeVer unknown_node_ver = {
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
        printf("version: unknown");
    } else {
        printf("version: %s", sver);
    }
    return;
}

static MEM_AUX_INIT(nodeinfo_relax_cache, sizeof(vnodeInfo_relax), 0);
vnodeInfo_relax* vnodeInfo_relax_alloc(void)
{
    vnodeInfo_relax* nodei = NULL;
    nodei = (vnodeInfo_relax*)vmem_aux_alloc(&nodeinfo_relax_cache);
    retE_p((!nodei));
    memset(nodei, 0, sizeof(*nodei));
    return nodei;
}

void vnodeInfo_relax_free(vnodeInfo_relax* nodei)
{
    vassert(nodei);

    vmem_aux_free(&nodeinfo_relax_cache, nodei);
    return ;
}

int vnodeInfo_relax_init(vnodeInfo_relax* nodei, vnodeId* id, vnodeVer* ver, int weight)
{
    vassert(nodei);
    vassert(id);
    vassert(ver);
    vassert(weight >= 0);

    vtoken_copy(&nodei->id,  id );
    vtoken_copy(&nodei->ver, ver);
    nodei->weight = (int32_t)weight;
    nodei->naddrs = 0;
    nodei->capc   = 12;

    return 0;
}

vnodeInfo_compact* vnodeInfo_compact_alloc(void)
{
    vnodeInfo_compact* nodei = NULL;
    nodei = (vnodeInfo_compact*)malloc(sizeof(*nodei));
    retE_p((!nodei));
    memset(nodei, 0, sizeof(*nodei));
    return nodei;
}

void vnodeInfo_compact_free(vnodeInfo_compact* nodei)
{
    vassert(nodei);

    free(nodei);
    return ;
}

int vnodeInfo_compact_init(vnodeInfo_compact* nodei, vnodeId* id, vnodeVer* ver, int weight)
{
    vassert(nodei);
    vassert(id);
    vassert(ver);
    vassert(weight >= 0);

    vtoken_copy(&nodei->id,  id );
    vtoken_copy(&nodei->ver, ver);
    nodei->weight = (int32_t)weight;
    nodei->naddrs = 0;
    nodei->capc   = 4;

    return 0;
}

int vnodeInfo_add_addr(vnodeInfo* nodei, struct sockaddr_in* addr)
{
    int found = 0;
    int i = 0;

    vassert(nodei);
    vassert(addr);

    retE((nodei->naddrs >= VNODEINFO_MAX_ADDRS));

    if (nodei->naddrs >= nodei->capc) {
        vnodeInfo* new_nodei = NULL;
        int extra_sz = sizeof(*addr) * nodei->capc;

        new_nodei = (vnodeInfo*)realloc(nodei, sizeof(vnodeInfo_compact) + extra_sz);
        vlog((!new_nodei), elog_realloc);
        retE((!new_nodei));

        nodei = new_nodei;
        nodei->capc += VNODEINFO_MIN_ADDRS;
    }
    for (i = 0; i < nodei->naddrs; i++) {
        if (vsockaddr_equal(&nodei->addrs[i], addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        vsockaddr_copy(&nodei->addrs[nodei->naddrs++], addr);
    }
    return (!found);
}

int vnodeInfo_has_addr(vnodeInfo* nodei, struct sockaddr_in* addr)
{
    int found = 0;
    int i = 0;

    vassert(nodei);
    vassert(addr);

    for (i = 0; i < nodei->naddrs; i++) {
        if (vsockaddr_equal(&nodei->addrs[i], addr)) {
            found = 1;
            break;
        }
    }
    return found;
}

int vnodeInfo_update(vnodeInfo* dest_nodei, vnodeInfo* src_nodei)
{
    int nupdts = 0;
    int naddrs = 0;
    int ret = 0;
    int i = 0;

    vassert(dest_nodei);
    vassert(src_nodei);

    if (vtoken_equal(&dest_nodei->ver, &unknown_node_ver)) {
        for (i = 0; i < src_nodei->naddrs; i++) {
            ret = vnodeInfo_add_addr(dest_nodei, &src_nodei->addrs[i]);
            retE((ret < 0));
            if (ret > 0) {
                nupdts++;
            }
        }
        return nupdts;
    }

    dest_nodei->weight = src_nodei->weight;
    if (dest_nodei->naddrs > src_nodei->naddrs) {
        naddrs = src_nodei->naddrs;
    } else {
        naddrs = dest_nodei->naddrs;
    }
    for (i = 0; i < naddrs; i++) {
        if (!vsockaddr_equal(&dest_nodei->addrs[i], &src_nodei->addrs[i])) {
            vsockaddr_copy(&dest_nodei->addrs[i], &src_nodei->addrs[i]);
            nupdts++;
        }
    }
    if (dest_nodei->naddrs > src_nodei->naddrs) {
        dest_nodei->naddrs = src_nodei->naddrs;
    } else {
        for (i = dest_nodei->naddrs; i < src_nodei->naddrs; i++) {
            ret = vnodeInfo_add_addr(dest_nodei, &src_nodei->addrs[i]);
            if (ret > 0) {
                nupdts++;
            }
        }
    }
    return nupdts;
}

int vnodeInfo_copy(vnodeInfo* dest_nodei, vnodeInfo* src_nodei)
{
    int i = 0;

    vassert(dest_nodei);
    vassert(src_nodei);

    vtoken_copy(&dest_nodei->id,  &src_nodei->id);
    vtoken_copy(&dest_nodei->ver, &src_nodei->ver);
    dest_nodei->weight = src_nodei->weight;
    dest_nodei->naddrs = 0;

    for (i = 0; i < src_nodei->naddrs; i++) {
        vnodeInfo_add_addr(dest_nodei, &src_nodei->addrs[i]);
    }
    return 0;
}

void vnodeInfo_dump(vnodeInfo* nodei)
{
    int i = 0;
    vassert(nodei);

    vtoken_dump(&nodei->id);
    printf(", ");
    vnodeVer_dump(&nodei->ver);
    printf(", ");
    printf("weight: %d,", nodei->weight);
    vsockaddr_dump(&nodei->addrs[0]);
    for (i = 1; i < nodei->naddrs; i++) {
        printf(", ");
        vsockaddr_dump(&nodei->addrs[i]);
    }
    return ;
}

/*
 * for vnodeConn
 */
int vnodeConn_set(vnodeConn* conn, struct sockaddr_in* local, struct sockaddr_in* remote)
{
    vassert(conn);
    vassert(local);
    vassert(remote);

    memset(conn, 0, sizeof(*conn));
    vsockaddr_copy(&conn->local,  local);
    vsockaddr_copy(&conn->remote, remote);

    if (vsockaddr_is_private(remote)) {
        conn->weight = VNODECONN_WEIGHT_HIGHT;
    } else {
        conn->weight = VNODECONN_WEIGHT_LOW;
    }
    return 0;
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
vsrvcInfo* vsrvcInfo_alloc(void)
{
    vsrvcInfo* info = NULL;

    info = (vsrvcInfo*)malloc(sizeof(vsrvcInfo));
    vlog((!info), elog_malloc);
    retE_p((!info));

    memset(info, 0, sizeof(*info));
    info->naddrs = 0;
    info->capc   = 2;
    return info;
}

vsrvcInfo* vsrvcInfo_dup(vsrvcInfo* svc_info)
{
    vsrvcInfo* svc_dup = NULL;
    int i = 0;
    vassert(svc_info);

    svc_dup = vsrvcInfo_alloc();
    vlog((!svc_dup), elog_vsrvcInfo_alloc);
    retE_p((!svc_dup));

    vsrvcInfo_init(svc_dup, &svc_info->id, svc_info->nice);
    for (i = 0; i < svc_info->naddrs; i++) {
        vsrvcInfo_add_addr(svc_dup, &svc_info->addr[i]);
    }
    return svc_dup;
}

void vsrvcInfo_free(vsrvcInfo* svc_info)
{
    vassert(svc_info);
    free(svc_info);
    return ;
}

int vsrvcInfo_init(vsrvcInfo* svc_info, vsrvcId* srvcId, int32_t nice)
{
    vassert(svc_info);
    vassert(srvcId);
    vassert(nice >= 0);

    vtoken_copy(&svc_info->id, srvcId);
    svc_info->nice = nice;
    return 0;
}

int vsrvcInfo_add_addr(vsrvcInfo* svc_info, struct sockaddr_in* addr)
{
    vsrvcInfo* info = NULL;
    int capc = svc_info->capc << 1;
    int found = 0;
    int i = 0;
    vassert(svc_info);
    vassert(addr);

    if (svc_info->naddrs >= svc_info->capc) {
        info = (vsrvcInfo*)realloc(svc_info, sizeof(vsrvcInfo) + capc);
        vlog((!info), elog_realloc);
        retE((!info));

        svc_info = info;
        svc_info->capc = capc;
    }
    for (i = 0; i < svc_info->naddrs; i++) {
        if (vsockaddr_equal(addr, &svc_info->addr[i])) {
            found = 1;
            break;
        }
    }
    if (!found) {
        vsockaddr_copy(&svc_info->addr[svc_info->naddrs++], addr);
    }
    return 0;
}

void vsrvcInfo_del_addr(vsrvcInfo* svc_info, struct sockaddr_in* addr)
{
    int found = 0;
    int i = 0;
    vassert(svc_info);
    vassert(addr);

    for (i = 0; i < svc_info->naddrs; i++) {
        if (vsockaddr_equal(addr, &svc_info->addr[i])) {
            found = 1;
            break;
        }
    }
    if (found) {
        for (; i < svc_info->naddrs; i++) {
            vsockaddr_copy(&svc_info->addr[i], &svc_info->addr[i+1]);
        }
    }
    return ;
}

int vsrvcInfo_is_empty(vsrvcInfo* svc_info)
{
    vassert(svc_info);
    return !(svc_info->naddrs);
}

int vsrvcInfo_equal(vsrvcInfo* a, vsrvcInfo* b)
{
    vassert(a);
    vassert(b);

    return vtoken_equal(&a->id, &b->id);
}

void vsrvcInfo_dump(vsrvcInfo* svc_info)
{
    int i = 0;
    vassert(svc_info);

    vtoken_dump(&svc_info->id);
    printf(", nice: %d", svc_info->nice);
    printf(", addrs: ");
    vsockaddr_dump(&svc_info->addr[0]);
    for (i = 1; i < svc_info->naddrs; i++) {
        printf(", ");
        vsockaddr_dump(&svc_info->addr[i]);

    }
    return ;
}

