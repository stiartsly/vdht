#ifndef __VNODE_ID_H__
#define __VNODE_ID_H__

#include <netinet/in.h>

#define VTOKEN_LEN    20
#define VTOKEN_INTLEN 5
#define VTOKEN_BITLEN 160

struct vtoken {
    uint8_t data[VTOKEN_LEN];
};
typedef struct vtoken vtoken;

void vtoken_make   (vtoken*);
int  vtoken_equal  (vtoken*, vtoken*);
void vtoken_copy   (vtoken*, vtoken*);
void vtoken_dump   (vtoken*);
int  vtoken_strlize(vtoken*, char*, int);
int  vtoken_unstrlize(const char*, vtoken*);

/*
 * for vnodeId
 */
typedef struct vtoken vnodeId;
typedef struct vtoken vnodeMetric;

void vnodeId_dist  (vnodeId*, vnodeId*, vnodeMetric*);
int  vnodeId_bucket(vnodeId*, vnodeId*);
int  vnodeMetric_cmp(vnodeMetric*, vnodeMetric*);

/*
 * for vnodeHash
 */
typedef struct vtoken vnodeHash;

/*
 * for vnodeVer
 */
typedef struct vtoken vnodeVer;

void vnodeVer_dump     (vnodeVer*);
int  vnodeVer_strlize  (vnodeVer*, char*, int);
int  vnodeVer_unstrlize(const char*, vnodeVer*);
extern vnodeVer unknown_node_ver;

/*
 * for vnodeInfo
 */
enum {
    VNODEINFO_LADDR = (uint32_t)(1 << 0),
    VNODEINFO_UADDR = (uint32_t)(1 << 1),
    VNODEINFO_EADDR = (uint32_t)(1 << 2),
    VNODEINFO_RADDR = (uint32_t)(1 << 3)
};

struct vnodeInfo  {
    vnodeId  id;
    vnodeVer ver;
    int32_t weight;

    struct sockaddr_in laddr; // local address
    struct sockaddr_in uaddr; // upnp  address
    struct sockaddr_in eaddr; // external address.
    struct sockaddr_in raddr; // relay address
    uint32_t addr_flags;
};
typedef struct vnodeInfo vnodeInfo;

vnodeInfo* vnodeInfo_alloc(void);
void vnodeInfo_free (vnodeInfo*);
int  vnodeInfo_equal(vnodeInfo*, vnodeInfo*);
void vnodeInfo_copy (vnodeInfo*, vnodeInfo*);
int  vnodeInfo_init (vnodeInfo*, vnodeId*, vnodeVer*, int32_t);
void vnodeInfo_set_laddr(vnodeInfo*, struct sockaddr_in*);
void vnodeInfo_set_uaddr(vnodeInfo*, struct sockaddr_in*);
void vnodeInfo_set_eaddr(vnodeInfo*, struct sockaddr_in*);
void vnodeInfo_dump (vnodeInfo*);
int  vnodeInfo_has_addr(vnodeInfo*, struct sockaddr_in*);

/*
 * for vsrvcId
 */
typedef struct vtoken vsrvcId;
int vsrvcId_bucket(vsrvcId*);

/*
 * for vsrvcInfo
 */
struct vsrvcInfo {
   vsrvcId id;
   int32_t nice;

   int32_t naddrs;
   int32_t capc;
   struct sockaddr_in addr[2];
};
typedef struct vsrvcInfo vsrvcInfo;
typedef void (*vsrvcInfo_iterate_addr_t)(struct sockaddr_in*, void*);

vsrvcInfo* vsrvcInfo_alloc(void);
vsrvcInfo* vsrvcInfo_dup  (vsrvcInfo*);
void vsrvcInfo_free (vsrvcInfo*);
int  vsrvcInfo_init (vsrvcInfo*, vsrvcId*, int32_t);
int  vsrvcInfo_add_addr(vsrvcInfo*, struct sockaddr_in*);
void vsrvcInfo_del_addr(vsrvcInfo*, struct sockaddr_in*);
int  vsrvcInfo_is_empty(vsrvcInfo*);
int  vsrvcInfo_equal(vsrvcInfo*, vsrvcInfo*);
void vsrvcInfo_dump (vsrvcInfo*);

#endif

