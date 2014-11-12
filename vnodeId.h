#ifndef __VNODE_ID_H__
#define __VNODE_ID_H__

#include <netinet/in.h>

#define VNODE_ID_LEN    20
#define VNODE_ID_INTLEN 5
#define VNODE_ID_BITLEN 160
#define VTOKEN_LEN  VNODE_ID_LEN

/*
 * vnodeId
 */
struct vnodeId {
    unsigned char data[VNODE_ID_LEN];
};
typedef struct vnodeId vnodeId;
typedef struct vnodeId vnodeMetric;
typedef struct vnodeId vnodeVer;
typedef struct vnodeId vnodeHash;
typedef struct vnodeId vtoken;

/*
 * vnode addr
 */

struct vnodeAddr {
    vnodeId id;
    struct sockaddr_in addr;
};
typedef struct vnodeAddr vnodeAddr;

/*
 * vnode info
 */
struct vnodeInfo  {
    vnodeId  id;
    struct sockaddr_in addr;
    vnodeVer ver;
    uint32_t flags;
};
typedef struct vnodeInfo vnodeInfo;


/*
 * vnodeId funcs
 */
void vnodeId_make  (vnodeId*);
void vnodeId_dist  (vnodeId*, vnodeId*, vnodeMetric*);
int  vnodeId_bucket(vnodeId*, vnodeId*);
int  vnodeId_equal (vnodeId*, vnodeId*);
void vnodeId_dump  (vnodeId*);
void vnodeId_copy  (vnodeId*, vnodeId*);
int  vnodeId_strlize(vnodeId*, char*, int);
int  vnodeId_unstrlize(const char*, vnodeId*);

/*
 * vnodeMetric
 */
int  vnodeMetric_cmp(vnodeMetric*, vnodeMetric*);


/*
 * vnodeAddr funcs
 */
int  vnodeAddr_equal(vnodeAddr*, vnodeAddr*);
void vnodeAddr_dump (vnodeAddr*);
void vnodeAddr_copy (vnodeAddr*, vnodeAddr*);
int  vnodeAddr_init (vnodeAddr*, vnodeId*, struct sockaddr_in*);

/*
 * version
 */
void vnodeVer_copy   (vnodeVer*, vnodeVer*);
int  vnodeVer_equal  (vnodeVer*, vnodeVer*);
int  vnodeVer_strlize(vnodeVer*, char*, int);
int  vnodeVer_unstrlize(const char*, vnodeVer*);

/*
 * token
 */
void vtoken_make   (vtoken*);
void vtoken_copy   (vtoken*, vtoken*);
int  vtoken_equal  (vtoken*, vtoken*);
int  vtoken_strlize(vtoken*, char*, int);
int  vtoken_unstrlize(const char*, vtoken*);
void vtoken_dump   (vtoken*);

/*
 * vnodeInfo
 */
vnodeInfo* vnodeInfo_alloc(void);
void vnodeInfo_free(vnodeInfo*);
int  vnodeInfo_init(vnodeInfo*, vnodeId*, struct sockaddr_in*, uint32_t, vnodeVer*);

#endif
