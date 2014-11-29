#ifndef __VNODE_ID_H__
#define __VNODE_ID_H__

#include <netinet/in.h>

#define VTOKEN_LEN    20
#define VTOKEN_INTLEN 5
#define VTOKEN_BITLEN 160

struct vtoken {
    unsigned char data[VTOKEN_LEN];
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

/*
 * for vnodeAddr
 */
struct vnodeAddr {
    vnodeId id;
    struct sockaddr_in addr;
};
typedef struct vnodeAddr vnodeAddr;

int  vnodeAddr_equal(vnodeAddr*, vnodeAddr*);
void vnodeAddr_dump (vnodeAddr*);
void vnodeAddr_copy (vnodeAddr*, vnodeAddr*);
int  vnodeAddr_init (vnodeAddr*, vnodeId*, struct sockaddr_in*);

/*
 * for vnodeInfo
 */
struct vnodeInfo  {
    vnodeId  id;
    struct sockaddr_in addr;
    vnodeVer ver;
    uint32_t flags;
};
typedef struct vnodeInfo vnodeInfo;

vnodeInfo* vnodeInfo_alloc(void);
void vnodeInfo_free(vnodeInfo*);
void vnodeInfo_copy(vnodeInfo*, vnodeInfo*);
int  vnodeInfo_init(vnodeInfo*, vnodeId*, struct sockaddr_in*, uint32_t, vnodeVer*);
void vnodeInfo_dump(vnodeInfo*);

/*
 * for vsrvcId
 */
typedef struct vtoken vsrvcId;

/*
 * for vsrvcInfo
 */
 struct vsrvcInfo {
    vsrvcId id;
    struct sockaddr_in addr;
    int usage;
 };
 typedef struct vsrvcInfo vsrvcInfo;

 vsrvcInfo* vsrvcInfo_alloc(void);
 void vsrvcInfo_free(vsrvcInfo*);
 int  vsrvcInfo_init(vsrvcInfo*, int, struct sockaddr_in*);
 void vsrvcInfo_dump(vsrvcInfo*);

#endif

