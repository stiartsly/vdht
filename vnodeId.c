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

    nodei->capc = VNODEINFO_MAX_ADDRS;
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
    nodei->capc   = VNODEINFO_MAX_ADDRS;

    return 0;
}

vnodeInfo* vnodeInfo_alloc(void)
{
    vnodeInfo* nodei = NULL;
    nodei = (vnodeInfo*)malloc(sizeof(*nodei));
    retE_p((!nodei));
    memset(nodei, 0, sizeof(*nodei));

    nodei->capc = VNODEINFO_MIN_ADDRS;
    return nodei;
}

void vnodeInfo_free(vnodeInfo* nodei)
{
    vassert(nodei);

    free(nodei);
    return ;
}

int vnodeInfo_init(vnodeInfo* nodei, vnodeId* id, vnodeVer* ver, int weight)
{
    vassert(nodei);
    vassert(id);
    vassert(ver);
    vassert(weight >= 0);

    vtoken_copy(&nodei->id,  id );
    vtoken_copy(&nodei->ver, ver);
    nodei->weight = (int32_t)weight;
    nodei->naddrs = 0;
    nodei->capc   = VNODEINFO_MIN_ADDRS;

    return 0;
}

int vnodeInfo_add_addr(vnodeInfo** ppnodei, struct sockaddr_in* addr)
{
    vnodeInfo* nodei = *ppnodei;
    int found = 0;
    int i = 0;

    vassert(*ppnodei);
    vassert(addr);

    retE((nodei->naddrs >= VNODEINFO_MAX_ADDRS));

    if (nodei->naddrs >= nodei->capc) {
        vnodeInfo* new_nodei = NULL;
        int extra_sz = sizeof(*addr) * nodei->capc;

        new_nodei = (vnodeInfo*)realloc(nodei, sizeof(vnodeInfo) + extra_sz);
        vlog((!new_nodei), elog_realloc);
        retE((!new_nodei));

        nodei = *ppnodei =  new_nodei;
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

int vnodeInfo_copy(vnodeInfo* dest, vnodeInfo* src)
{
    int updted = 0;
    int ret = 0;
    int i = 0;

    vassert(dest);
    vassert(src);

    vtoken_copy(&dest->id,  &src->id);
    vtoken_copy(&dest->ver, &src->ver);
    dest->weight = src->weight;
    dest->naddrs = 0;

    for (i = 0; i < src->naddrs; i++) {
        ret = vnodeInfo_add_addr(&dest, &src->addrs[i]);
        retE((ret < 0));
        if (ret > 0) {
            updted = 1;
        }
    }
    return ((ret > 0) ? updted : ret);
}

int vnodeInfo_update(vnodeInfo* dest, vnodeInfo* src)
{
    int updted = 0;
    int ret = 0;
    int i = 0;

    vassert(dest);
    vassert(src);

    if (vtoken_equal(&src->ver, &unknown_node_ver)) {
        // only interested in addresses
        for (i = 0; i < src->naddrs; i++) {
            ret = vnodeInfo_add_addr(&dest, &src->addrs[i]);
            retE((ret < 0));
            if (ret > 0) {
                updted = 1;
            }
        }
        return updted;
    }

    //node ID should be same value;
    vassert(vtoken_equal(&dest->id, &src->id));
    ret = vnodeInfo_copy(dest, src);
    retE((ret < 0));
    if (ret > 0) {
        updted = 1;
    }
    return ((ret > 0) ? updted : ret);
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
    printf("\n");
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

int vnodeConn_adjust(vnodeConn* old, vnodeConn* new)
{
    vassert(old);
    vassert(new);

    if (old->weight >= new->weight) {
        return 0;
    }
    old->weight = new->weight;
    vsockaddr_copy(&old->local,  &new->local);
    vsockaddr_copy(&new->remote, &new->remote);

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
int  vsrvcInfo_relax_init(vsrvcInfo_relax* srvci, vsrvcId* srvcId, vsrvcHash* hash, int nice)
{
    vassert(srvci);
    vassert(srvcId);
    vassert(hash);
    vassert(nice >= 0);

    vtoken_copy(&srvci->id, srvcId);
    vtoken_copy(&srvci->hash, hash);
    srvci->nice = nice;
    srvci->naddrs = 0;
    srvci->capc   = VSRVCINFO_MAX_ADDRS;

    return 0;
}

vsrvcInfo* vsrvcInfo_alloc(void)
{
    vsrvcInfo* srvci = NULL;

    srvci = (vsrvcInfo*)malloc(sizeof(vsrvcInfo));
    vlog((!srvci), elog_malloc);
    retE_p((!srvci));

    memset(srvci, 0, sizeof(*srvci));
    srvci->naddrs = 0;
    srvci->capc   = VSRVCINFO_MIN_ADDRS;
    return srvci;
}

void vsrvcInfo_free(vsrvcInfo* srvci)
{
    vassert(srvci);

    free(srvci);
    return ;
}

int vsrvcInfo_init(vsrvcInfo* srvci, vsrvcId* id, vsrvcHash* hash, int nice)
{
    vassert(srvci);
    vassert(id);
    vassert(hash);
    vassert(nice >= 0);

    vtoken_copy(&srvci->id,   id);
    vtoken_copy(&srvci->hash, hash);
    srvci->nice = nice;

    srvci->naddrs = 0;
    srvci->capc   = VSRVCINFO_MIN_ADDRS;

    return 0;
}

int vsrvcInfo_add_addr(vsrvcInfo** ppsrvci, struct sockaddr_in* addr)
{
    vsrvcInfo* srvci = *ppsrvci;
    int found = 0;
    int i = 0;

    vassert(*ppsrvci);
    vassert(addr);

    retE((srvci->naddrs >= VSRVCINFO_MAX_ADDRS));

    if (srvci->naddrs >= srvci->capc) {
        vsrvcInfo* new_srvci = NULL;
        int extra_sz = sizeof(*addr) * srvci->capc;

        new_srvci = (vsrvcInfo*)realloc(srvci, sizeof(vsrvcInfo) + extra_sz);
        vlog((!new_srvci), elog_realloc);
        retE((!new_srvci));

        srvci = *ppsrvci = new_srvci;
        srvci->capc += VSRVCINFO_MIN_ADDRS;
    }

    for (i = 0; i < srvci->naddrs; i++) {
        if (vsockaddr_equal(&srvci->addrs[i], addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        vsockaddr_copy(&srvci->addrs[srvci->naddrs++], addr);
    }
    return 0;
}

void vsrvcInfo_del_addr(vsrvcInfo* srvci, struct sockaddr_in* addr)
{
    int found = 0;
    int i = 0;

    vassert(srvci);
    vassert(addr);

    for (i = 0; i < srvci->naddrs; i++) {
        if (vsockaddr_equal(&srvci->addrs[i], addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return ;
    }

    for (; i < srvci->naddrs -1; i++) {
        vsockaddr_copy(&srvci->addrs[i], &srvci->addrs[i+1]);
    }
    srvci->naddrs -= 1;
    return ;
}

int vsrvcInfo_is_empty(vsrvcInfo* srvci)
{
    vassert(srvci);

    return (!(srvci->naddrs));
}

int vsrvcInfo_copy(vsrvcInfo* dest, vsrvcInfo* src)
{
    int ret = 0;
    int i = 0;

    vassert(dest);
    vassert(src);

    vtoken_copy(&dest->id,   &src->id);
    vtoken_copy(&dest->hash, &src->hash);
    dest->nice   = src->nice;
    dest->naddrs = 0;

    for (i = 0; i < src->naddrs; i++) {
        ret = vsrvcInfo_add_addr(&dest, &src->addrs[i]);
        retE((ret < 0));
    }
    return 0;
}

void vsrvcInfo_dump(vsrvcInfo* srvci)
{
    int i = 0;
    vassert(srvci);

    vtoken_dump(&srvci->hash);
    printf(", nice: %d", srvci->nice);
    printf(", addrs: ");
    vsockaddr_dump(&srvci->addrs[0]);
    for (i = 1; i < srvci->naddrs; i++) {
        printf(", ");
        vsockaddr_dump(&srvci->addrs[i]);

    }
    return ;
}

