#include "vglobal.h"
#include "vnodeId.h"

static uint32_t nonce_dynamic_magic = 0;
void vnonce_make(vnonce* nonce)
{
    uint32_t seed = (uint32_t)time(NULL);
    vassert(nonce);

    seed += nonce_dynamic_magic;
    nonce_dynamic_magic = seed;
    srand(seed);

    *((uint32_t*)nonce->data) = (uint32_t)rand();
    return ;
}

int vnonce_equal(vnonce* a, vnonce* b)
{
    vassert(a);
    vassert(b);
    return (*((uint32_t*)a->data) == *((uint32_t*)b->data));
}

void vnonce_copy(vnonce* dst, vnonce* src)
{
    vassert(dst);
    vassert(src);

    *((uint32_t*)dst->data) = *((uint32_t*)src->data);
    return ;
}

int vnonce_strlize(vnonce* nonce, char* buf, int len)
{
    uint8_t data = 0;
    int ret = 0;
    int sz  = 0;
    int i   = 0;

    vassert(nonce);
    vassert(buf);
    vassert(len > 0);

    for (; i < VNONCE_LEN; i++) {
        data = nonce->data[i];
        ret = snprintf(buf+sz, len-sz, "%x%x", (data >> 4), (data & 0x0f));
        vlogEv((ret >= len-sz), elog_snprintf);
        retE((ret >= len-sz));
        sz += ret;
    }

    return 0;
}

int vnonce_unstrlize(const char* id_str, vnonce* nonce)
{
    uint32_t low  = 0;
    uint32_t high = 0;
    char data[2] = {'\0', '\0'};
    int ret = 0;
    int i = 0;
    vassert(id_str);
    vassert(nonce);

    for (; i < VNONCE_LEN; i++) {
        data[0] = *id_str++;
        ret = sscanf(data, "%x", &high);
        vlogEv((!ret), elog_sscanf);
        retE((!ret));

        data[0] = *id_str++;
        ret = sscanf(data, "%x", &low);
        vlogEv((!ret), elog_sscanf);
        retE((!ret));

        nonce->data[i] = ((high << 4) | (0x0f & low));
    }
    return 0;
}

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
        vlogEv((ret < 0), elog_vmacaddr_get);
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
        vlogEv((ret >= len-sz), elog_snprintf);
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
        vlogEv((!ret), elog_sscanf);
        retE((!ret));

        data[0] = *id_str++;
        ret = sscanf(data, "%x", &low);
        vlogEv((!ret), elog_sscanf);
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

vnodeVer* vnodeVer_unknown(void)
{
    static vnodeVer unknown_ver = {.data = {0, 0, 0, 0, 0}};
    return &unknown_ver;
}

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
        vlogEv((ret >= len-offset), elog_snprintf);
        retE((ret >= len-offset));
        offset += ret;

        ret = snprintf(buf + offset, len - offset, "%d", ver->data[i]);
        vlogEv((ret >= len-offset), elog_snprintf);
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
    vlogEv((errno), elog_strtol);
    retE((errno));
    ver->data[i] = ret;
    i++;

    while(*s != '\0') {
        retE((*s != '.'));
        s++;

        errno = 0;
        ret = strtol(s, &s, 10);
        vlogEv((errno), elog_strtol);
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

static
const char* vsockaddr_in_desc(uint32_t type)
{
    struct vsockaddr_desc {
        uint32_t type;
        const char* desc;
    } sockaddr_descs[] = {
        {VSOCKADDR_LOCAL,     "host" },
        {VSOCKADDR_UPNP,      "upnp" },
        {VSOCKADDR_REFLEXIVE, "rflx" },
        {VSOCKADDR_RELAY,     "relay"},
        {VSOCKADDR_BUTT, 0}
    };

    struct vsockaddr_desc* desc = sockaddr_descs;
    for (; desc->desc; desc++) {
        if (VSOCKADDR_TYPE(type) == desc->type) {
            return desc->desc;
        }
    }
    return "unknown";
}

static MEM_AUX_INIT(nodeinfo_relax_cache, sizeof(vnodeInfo_relax), 0);
vnodeInfo* vnodeInfo_relax_alloc(void)
{
    vnodeInfo* nodei = NULL;
    nodei = (vnodeInfo*)vmem_aux_alloc(&nodeinfo_relax_cache);
    retE_p((!nodei));
    memset(nodei, 0, sizeof(vnodeInfo_relax));

    nodei->capc = VNODEINFO_MAX_ADDRS;
    return nodei;
}

void vnodeInfo_relax_free(vnodeInfo* nodei)
{
    vassert(nodei);

    vmem_aux_free(&nodeinfo_relax_cache, nodei);
    return ;
}

int vnodeInfo_relax_init(vnodeInfo* nodei, vnodeId* id, vnodeVer* ver, int weight)
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
    vlogEv((!nodei), elog_malloc);
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

int vnodeInfo_add_addr(vnodeInfo** ppnodei, struct sockaddr_in* addr, uint32_t type)
{
    vnodeInfo* nodei = *ppnodei;
    int found = 0;
    int i = 0;

    vassert(*ppnodei);
    vassert(addr);

    retE((nodei->naddrs >= VNODEINFO_MAX_ADDRS));

    /*
     * if the number of addresses is about to exceed capacitiy, then realloc
     * 'nodei' to ship more addresses.
     */
    if (nodei->naddrs >= nodei->capc) {
        vnodeInfo* new_nodei = NULL;
        int extra_sz = sizeof(struct vsockaddr_in) * nodei->capc;

        new_nodei = (vnodeInfo*)realloc(nodei, sizeof(vnodeInfo) + extra_sz);
        vlogEv((!new_nodei), elog_realloc);
        retE((!new_nodei));

        nodei = *ppnodei =  new_nodei;
        nodei->capc += VNODEINFO_MIN_ADDRS;
    }

    for (i = 0; i < nodei->naddrs; i++) {
        if (!vsockaddr_equal(&nodei->addrs[i].addr, addr)) {
            continue;
        }
        if (vsockaddr_equal(&nodei->addrs[i].addr, addr)) {
            if ((VSOCKADDR_TYPE(type) != VSOCKADDR_UNKNOWN) &&
                (VSOCKADDR_TYPE(nodei->addrs[i].type) != VSOCKADDR_TYPE(type))) {
                //todo;
                nodei->addrs[i].type = type;
            }
            found = 1;
            break;
        }
    }

    // insert the address according to it's type value.
    if (!found) {
        for (i = nodei->naddrs; i > 0; i--) {
            if (VSOCKADDR_TYPE(nodei->addrs[i-1].type) > VSOCKADDR_TYPE(type)) {
                vsockaddr_copy(&nodei->addrs[i].addr, &nodei->addrs[i-1].addr);
                nodei->addrs[i].type = nodei->addrs[i-1].type;
            }else {
                break;
            }
        }
        vsockaddr_copy(&nodei->addrs[i].addr, addr);
        nodei->addrs[i].type = type;
        nodei->naddrs++;
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
        if (vsockaddr_equal(&nodei->addrs[i].addr, addr)) {
            found = 1;
            break;
        }
    }
    return found;
}

int vnodeInfo_copy(vnodeInfo** ppdest, vnodeInfo* src)
{
    vnodeInfo* dest = *ppdest;
    int updated = 0;
    int ret = 0;
    int i = 0;

    vassert(dest);
    vassert(src);

    vtoken_copy(&dest->id,  &src->id );
    vtoken_copy(&dest->ver, &src->ver);
    dest->weight = src->weight;
    dest->naddrs = 0;

    memset(dest->addrs, 0, sizeof(struct vsockaddr_in)* dest->capc);
    for (i = 0; i < src->naddrs; i++) {
        ret = vnodeInfo_add_addr(ppdest, &src->addrs[i].addr, src->addrs[i].type);
        retE((ret < 0));
        updated++;
    }
    return (!!updated);
}

int vnodeInfo_merge(vnodeInfo** ppdest, vnodeInfo* src)
{
    vnodeInfo* dest = *ppdest;
    int updated = 0;
    int ret = 0;
    int i   = 0;

    vassert(dest);
    vassert(src);

    if (!vtoken_equal(&dest->id, &src->id)) {
        return 0;
    }
    if (vnodeInfo_is_fake(src)) {
        /*
         * if source is DHT nodei with unknown version ( normally due to
         * receiving 'ping' query from a DHT node), then only the address
         * is interested to 'dest' DHT nodei;
         */
        for (i = 0; i < src->naddrs; i++) {
            if (vnodeInfo_has_addr(dest, &src->addrs[i].addr) > 0) {
                continue;
            }
            ret = vnodeInfo_add_addr(ppdest, &src->addrs[i].addr, src->addrs[i].type);
            retE((ret < 0));
            updated++;
        }
        return (!!updated);
    }
    /*
     * if 'src' DHT nodei has diffrent number of addresses from 'dest' DHT
     * nodei, then it means that DHT node to 'dest' might be updated for
     * some reasons, and 'dest' nodei need to be updated accordingly.
     */
    if (dest->naddrs != src->naddrs) {
        ret = vnodeInfo_copy(ppdest, src);
        retE((ret < 0));
        return 1;
    }

    /*
     * if 'src' DHT nodei has same number of addresses with 'dest' DHT nodei,
     * just compare each address and update it.
     */
    vtoken_copy(&dest->ver, &src->ver);
    dest->weight = src->weight;
    for (i = 0; i < src->naddrs; i++) {
        if (vsockaddr_equal(&dest->addrs[i].addr, &src->addrs[i].addr)) {
            continue;
        }
        vsockaddr_copy(&dest->addrs[i].addr, &src->addrs[i].addr);
        dest->addrs[i].type = VSOCKADDR_TYPE(dest->addrs[i].type);
        updated++;
    }
    return (!!updated);
}

struct sockaddr_in* vnodeInfo_worst_addr(vnodeInfo* nodei)
{
    vassert(nodei);
    return &nodei->addrs[nodei->naddrs-1].addr;
}

int vnodeInfo_size(vnodeInfo* nodei)
{
    int sz = 0;
    vassert(nodei);

    sz += sizeof(vnodeId);
    sz += sizeof(vnodeVer);
    sz += sizeof(int32_t);
    sz += sizeof(int16_t) + sizeof(int16_t);
    sz += nodei->naddrs * sizeof(struct vsockaddr_in);

    return sz;
}

int vnodeInfo_is_fake(vnodeInfo* nodei)
{
    vassert(nodei);
    return (vtoken_equal(&nodei->ver, vnodeVer_unknown()));
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
    printf("addrs:");
    vsockaddr_dump(&nodei->addrs[0].addr);
    printf(" %s", vsockaddr_in_desc(VSOCKADDR_TYPE(nodei->addrs[0].type)));
    for (i = 1; i < nodei->naddrs; i++) {
        printf(", ");
        vsockaddr_dump(&nodei->addrs[i].addr);
        printf(" %s", vsockaddr_in_desc(VSOCKADDR_TYPE(nodei->addrs[i].type)));
    }
    printf("\n");
    return ;
}

/*
 * for vnodeConn
 */
void vnodeConn_set(vnodeConn* conn, struct vsockaddr_in* local, struct vsockaddr_in* remote)
{
    vassert(conn);
    vassert(local);
    vassert(remote);

    vsockaddr_copy(&conn->laddr, &local->addr );
    vsockaddr_copy(&conn->raddr, &remote->addr);
    conn->priority  = VSOCKADDR_TYPE(local->type);
    conn->priority += VSOCKADDR_TYPE(remote->type);
    return;
}

void vnodeConn_set_raw(vnodeConn* conn, struct sockaddr_in* laddr, struct sockaddr_in* raddr)
{
    vassert(conn);
    vassert(laddr);
    vassert(raddr);

    vsockaddr_copy(&conn->laddr, laddr);
    vsockaddr_copy(&conn->raddr, raddr);
    conn->priority  = VSOCKADDR_UNKNOWN;
    conn->priority += VSOCKADDR_UNKNOWN;
    return;
}

void vnodeConn_adjust(vnodeConn* old, vnodeConn* neo)
{
    vassert(old);
    vassert(neo);

    if (neo->priority > old->priority) {
        return ;
    }
    old->priority = neo->priority;
    vsockaddr_copy(&old->laddr, &neo->laddr);
    vsockaddr_copy(&old->raddr, &neo->raddr);
    return ;
}

/*
 * for vsrvcId
 */
int vsrvcId_bucket(vtoken* id)
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
int vsrvcInfo_relax_init(vsrvcInfo_relax* srvci, vsrvcHash* hash, vnodeId* hostid, int nice)
{
    vassert(srvci);
    vassert(hash);
    vassert(hostid);
    vassert(nice >= 0);

    vtoken_copy(&srvci->hash,  hash);
    vtoken_copy(&srvci->hostid,hostid);
    srvci->nice = nice;
    srvci->naddrs = 0;
    srvci->capc   = VSRVCINFO_MAX_ADDRS;

    return 0;
}

vsrvcInfo* vsrvcInfo_alloc(void)
{
    vsrvcInfo* srvci = NULL;

    srvci = (vsrvcInfo*)malloc(sizeof(vsrvcInfo));
    vlogEv((!srvci), elog_malloc);
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

int vsrvcInfo_init(vsrvcInfo* srvci, vsrvcHash* hash, vnodeId* hostid, int nice)
{
    vassert(srvci);
    vassert(hash);
    vassert(hostid);
    vassert(nice >= 0);

    vtoken_copy(&srvci->hash, hash);
    vtoken_copy(&srvci->hostid, hostid);

    srvci->nice = nice;
    srvci->naddrs = 0;
    srvci->capc   = VSRVCINFO_MIN_ADDRS;

    return 0;
}

int vsrvcInfo_add_addr(vsrvcInfo** ppsrvci, struct sockaddr_in* addr, uint32_t type)
{
    vsrvcInfo* srvci = *ppsrvci;
    int found = 0;
    int i = 0;

    vassert(*ppsrvci);
    vassert(addr);
    vassert(type < VSOCKADDR_BUTT);
    retE((srvci->naddrs >= VSRVCINFO_MAX_ADDRS));

    if (srvci->naddrs >= srvci->capc) {
        vsrvcInfo* new_srvci = NULL;
        int extra_sz = sizeof(struct vsockaddr_in) * srvci->capc;

        new_srvci = (vsrvcInfo*)realloc(srvci, sizeof(vsrvcInfo) + extra_sz);
        vlogEv((!new_srvci), elog_realloc);
        retE((!new_srvci));

        srvci = *ppsrvci = new_srvci;
        srvci->capc += VSRVCINFO_MIN_ADDRS;
    }

    // find whether already containing the address.
    for (i = 0; i < srvci->naddrs; i++) {
        if (vsockaddr_equal(&srvci->addrs[i].addr, addr)) {
            if (srvci->addrs[i].type != type) {
                //todo;
                srvci->addrs[i].type = type;
            }
            found = 1;
            break;
        }
    }
    // insert the address according to it's type value.
    if (!found) {
        for (i = srvci->naddrs; i > 0; i--) {
            if (srvci->addrs[i-1].type > type) {
                vsockaddr_copy(&srvci->addrs[i].addr, &srvci->addrs[i-1].addr);
                srvci->addrs[i].type = srvci->addrs[i-1].type;
            }else {
                break;
            }
        }
        vsockaddr_copy(&srvci->addrs[i].addr, addr);
        srvci->addrs[i].type = type;

        srvci->naddrs++;
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
        if (vsockaddr_equal(&srvci->addrs[i].addr, addr)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return ;
    }

    for (; i < srvci->naddrs -1; i++) {
        memcpy(&srvci->addrs[i], &srvci->addrs[i+1], sizeof(struct vsockaddr_in));
    }
    srvci->naddrs -= 1;
    return ;
}

int vsrvcInfo_is_empty(vsrvcInfo* srvci)
{
    vassert(srvci);

    return (!(srvci->naddrs));
}

int vsrvcInfo_size(vsrvcInfo* srvci)
{
    int sz = 0;
    vassert(srvci);

    sz += sizeof(vsrvcHash);
    sz += sizeof(vnodeId);
    sz += sizeof(int32_t);
    sz += sizeof(int16_t) + sizeof(int16_t);
    sz += srvci->naddrs * sizeof(struct vsockaddr_in);

    return sz;
}

int vsrvcInfo_copy(vsrvcInfo** ppdest, vsrvcInfo* src)
{
    vsrvcInfo* dest = *ppdest;
    int ret = 0;
    int i = 0;

    vassert(ppdest);
    vassert(src);

    vtoken_copy(&dest->hash,  &src->hash);
    vtoken_copy(&dest->hostid,&src->hostid);
    dest->nice   = src->nice;
    dest->naddrs = 0;

    for (i = 0; i < src->naddrs; i++) {
        ret = vsrvcInfo_add_addr(ppdest, &src->addrs[i].addr, src->addrs[i].type);
        retE((ret < 0));
    }
    return 0;
}

void vsrvcInfo_dump(vsrvcInfo* srvci)
{
    char* sproto = NULL;
    int i = 0;
    vassert(srvci);

    switch(vsrvcInfo_proto(srvci)) {
    case VPROTO_UDP:
        sproto = "udp";
        break;
    case VPROTO_TCP:
        sproto = "tcp";
        break;
    default:
        sproto = "err";
        break;
    }

    vtoken_dump(&srvci->hash);
    printf(", nice: %d", vsrvcInfo_nice(srvci));
    printf(", proto: %s", sproto);
    printf(", addrs: ");
    vsockaddr_dump(&srvci->addrs[0].addr);
    printf(" %s", vsockaddr_in_desc(srvci->addrs[0].type));
    for (i = 1; i < srvci->naddrs; i++) {
        printf(", ");
        vsockaddr_dump(&srvci->addrs[i].addr);
        printf(" %s", vsockaddr_in_desc(srvci->addrs[i].type));
    }
    return ;
}

