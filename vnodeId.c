#include "vglobal.h"
#include "vnodeId.h"

/*
 * for vtoken funcs
 */
void vtoken_make(vtoken* token)
{
    char* data = NULL;
    int i = 0;
    vassert(token);

    srand(time(NULL)); // TODO: need use mac as srand seed.
    data = (char*)token->data;
    for (; i < VTOKEN_LEN; i++) {
        data[i] = (char)(rand() % 16);
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
    int ret = 0;
    int sz  = 0;
    int i   = 0;

    vassert(token);
    vassert(buf);
    vassert(len > 0);

    memset(buf, 0, len);
    for (; i < VTOKEN_LEN; i++) {
        ret = snprintf(buf+sz, len-sz, "%x", token->data[i]);
        vlog((ret >= len-sz), elog_snprintf);
        retE((ret >= len-sz));
        sz += ret;
    }
    retE((strlen(buf) != VTOKEN_LEN));
    return 0;
}

int vtoken_unstrlize(const char* id_str, vtoken* token)
{
    char tmp[4];
    int ret = 0;
    int i = 0;
    vassert(id_str);
    vassert(token);

    memset(tmp, 0, 4);
    retE((strlen(id_str) != VTOKEN_LEN));
    for(; i < VTOKEN_LEN; i++) {
        int idata = 0;
        *tmp = id_str[i];
        ret = sscanf(tmp, "%x", &idata);
        vlog((!ret), elog_sscanf);
        retE((!ret));
        token->data[i] = (char)idata;
    }
    return 0;
}

void vtoken_dump(vtoken* token)
{
    char buf[64];
    int ret = 0;
    vassert(token);

    memset(buf, 0, 64);
    ret = snprintf(buf, 64, "ID:");
    vtoken_strlize(token, buf + ret, 64-ret);
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
 * for vnodeAddr funcs
 */
int  vnodeAddr_equal(vnodeAddr* a, vnodeAddr* b)
{
    vassert(a);
    vassert(b);

    return (vtoken_equal(&a->id, &b->id) || vsockaddr_equal(&a->addr,&b->addr));
}

void vnodeAddr_copy (vnodeAddr* a, vnodeAddr* b)
{
    vassert(a);
    vassert(b);

    vtoken_copy(&a->id, &b->id);
    vsockaddr_copy(&a->addr, &b->addr);
    return ;
}

void vnodeAddr_dump (vnodeAddr* addr)
{
    char ip[64];
    int port = 0;
    int ret = 0;

    vassert(addr);
    vtoken_dump(&addr->id);
    ret = vsockaddr_unconvert(&addr->addr, ip, 64, &port);
    vlog((ret < 0), elog_vsockaddr_unconvert);
    retE_v((ret < 0));
    printf("##Addr: %s:%d\n", ip, port);
    return ;
}

int vnodeAddr_init(vnodeAddr* nodeAddr, vnodeId* id, struct sockaddr_in* addr)
{
    vassert(nodeAddr);
    vassert(id);
    vassert(addr);

    vtoken_copy(&nodeAddr->id, id);
    vsockaddr_copy(&nodeAddr->addr, addr);
    return 0;
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

void vnodeInfo_copy(vnodeInfo* dest, vnodeInfo* src)
{
    vassert(dest);
    vassert(src);

    vtoken_copy(&dest->id, &src->id);
    vsockaddr_copy(&dest->addr, &src->addr);
    vtoken_copy(&dest->ver, &src->ver);
    dest->flags = src->flags;
    return ;
}

int vnodeInfo_init(vnodeInfo* info, vnodeId* id, struct sockaddr_in* addr, uint32_t flags, vnodeVer* ver)
{
    vassert(info);
    vassert(id);
    vassert(addr);
    vassert(ver || !ver);

    vtoken_copy(&info->id, id);
    vsockaddr_copy(&info->addr, addr);
    if (!ver) {
        vnodeVer_unstrlize("0.0.0.0.0", &info->ver);
    } else {
        vtoken_copy(&info->ver, ver);
    }
    info->flags = flags;
    return 0;
}

void vnodeInfo_dump(vnodeInfo* info)
{
    vassert(info);

    vtoken_dump(&info->id);
    vsockaddr_dump(&info->addr);
    vnodeVer_dump(&info->ver);
    vdump(printf("flags: 0x%x", info->flags));
    return ;
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

int vsrvcInfo_init(vsrvcInfo* svc_info, int what, int nice, struct sockaddr_in* addr)
{
    vassert(svc_info);
    vassert(addr);

    vtoken_make(&svc_info->id);
    vsockaddr_copy(&svc_info->addr, addr);
    svc_info->what = what;
    svc_info->nice = nice;
    return 0;
}

void vsrvcInfo_dump(vsrvcInfo* svc_info)
{
    vassert(svc_info);
    vtoken_dump(&svc_info->id);
    vsockaddr_dump(&svc_info->addr);
    vdump(printf("what:%d", svc_info->what));
    vdump(printf("nice:%d", svc_info->nice));

    return ;
}

