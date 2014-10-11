#include "vglobal.h"
#include "vnodeId.h"

void vnodeId_make(vnodeId* id)
{
    unsigned long* data = NULL;
    int i = 0;
    vassert(id);

    srand(time(NULL));
    data = (unsigned long*)id->data;
    for (; i < VNODE_ID_INTLEN; i++) {
        data[i] = rand();
    }
    return ;
}

void vnodeId_dist(vnodeId* a, vnodeId* b, vnodeMetric* m)
{
    int i = 0;
    vassert(a);
    vassert(b);
    vassert(m);

    unsigned char* a_data = a->data;
    unsigned char* b_data = b->data;
    unsigned char* m_data = m->data;

    for (; i < VNODE_ID_LEN; i++) {
        *(m_data++) = *(a_data++) ^ *(b_data++);
    }
    return ;
}

static
int _aux_distance(vnodeMetric* m)
{
    int i = 0;
    vassert(m);

    for (; i < VNODE_ID_BITLEN; i++) {
        int bit  = VNODE_ID_BITLEN - i - 1;
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

int vnodeId_equal(vnodeId* a, vnodeId* b)
{
    int i = 0;
    vassert(a);
    vassert(b);

    for (; i < VNODE_ID_LEN; i++) {
        if (a->data[i] != b->data[i]) {
            return 0;
        }
    }
    return 1;
}

void vnodeId_dump(vnodeId* id)
{
    vassert(id);
    //todo;
    return;
}

void vnodeId_copy(vnodeId* dst, vnodeId* src)
{
    int i = 0;
    vassert(dst);
    vassert(src);

    for (; i < VNODE_ID_LEN; i++) {
        dst->data[i] = src->data[i];
    }
    return ;
}

int vnodeId_strlize(vnodeId* id, char* buf, int len)
{
    vassert(id);
    vassert(buf);
    vassert(len > 0);

    //todo;
    return 0;
}

int vnodeId_unstrlize(const char* id_str, vnodeId* id)
{
    vassert(id_str);
    vassert(id);

    //todo;
    return 0;
}

/*
 *
 */
int vnodeMetric_cmp(vnodeMetric* a, vnodeMetric* b)
{
    unsigned char* a_data = a->data;
    unsigned char* b_data = b->data;
    int i = 0;

    vassert(a);
    vassert(b);

    for(; i < VNODE_ID_LEN; i++){
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
 * for vnode Addr.
 */
int  vnodeAddr_equal(vnodeAddr* a, vnodeAddr* b)
{
    vassert(a);
    vassert(b);

    if (vnodeId_equal(&a->id, &b->id) ||
        vsockaddr_equal(&a->addr,&b->addr)) {
        return 1;
    }
    return 0;
}

void vnodeAddr_copy (vnodeAddr* a, vnodeAddr* b)
{
    vassert(a);
    vassert(b);

    vnodeId_copy(&a->id, &b->id);
    vsockaddr_copy(&a->addr, &b->addr);
    return ;
}

void vnodeAddr_dump (vnodeAddr* addr)
{
    vassert(addr);
    //todo;
    return ;
}

int vnodeAddr_init(vnodeAddr* nodeAddr, vnodeId* id, struct sockaddr_in* addr)
{
    vassert(nodeAddr);
    vassert(id);
    vassert(addr);

    vnodeId_copy(&nodeAddr->id, id);
    vsockaddr_copy(&nodeAddr->addr, addr);
    return 0;
}

/*
 * for dht node version.
 */
void vnodeVer_copy(vnodeVer* dst, vnodeVer* src)
{
    vassert(dst);
    vassert(src);

    for (int i = 0; i < VNODE_ID_LEN; i++) {
        dst->data[i] = src->data[i];
    }
    return ;
}

int vnodeVer_equal(vnodeVer* ver_a, vnodeVer* ver_b)
{
    int i = 0;
    vassert(ver_a);
    vassert(ver_b);

    for (; i < VNODE_ID_LEN; i++) {
        if (ver_a->data[i] != ver_b->data[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 *  for token
 */
static 
vtoken gtoken = {{[0 ... VNODE_ID_INTLEN] = 0}};
void vtoken_make(vtoken* token)
{
#define VUINT32_MAX ((uint32_t)0x0ffffffe)
    uint32_t* data = NULL;
    int i = 0;
    vassert(token);

    data = (uint32_t*)gtoken.data;
    for (; i < VNODE_ID_INTLEN; i++) {
        if (data[i]++ >= VUINT32_MAX) {
            data[i] = 0;
        } else {
            break;
        }
    }
    return ;
}

int vtoken_equal(vtoken* a, vtoken* b)
{
    int i = 0;
    vassert(a);
    vassert(b);

    for (; i < VNODE_ID_LEN; i++) {
        if (a->data[i] != b->data[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 *
 */
static MEM_AUX_INIT(nodeInfo_maux, sizeof(vnodeInfo), 0);
vnodeInfo* vnodeInfo_alloc(void)
{
    vnodeInfo* info = NULL;

    info = (vnodeInfo*)vmem_aux_alloc(&nodeInfo_maux);
    ret1E_p((!info), elog_vmem_aux_alloc);
    return info;
}

void vnodeInfo_free(vnodeInfo* info)
{
    vmem_aux_free(&nodeInfo_maux, info);
    return ;
}

int vnodeInfo_init(vnodeInfo* info, vnodeId* id, struct sockaddr_in* addr, uint32_t flags, vnodeVer* ver)
{
    vassert(info);
    vassert(id);
    vassert(addr);
    vassert(ver);

    vnodeId_copy(&info->id, id);
    vsockaddr_copy(&info->addr, addr);
    vnodeVer_copy(&info->ver, ver);
    info->flags = flags;
    return 0;
}
