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
 * for vnodeVer
 */
typedef struct vtoken vnodeVer;

vnodeVer* vnodeVer_unknown(void);
int  vnodeVer_strlize  (vnodeVer*, char*, int);
int  vnodeVer_unstrlize(const char*, vnodeVer*);
void vnodeVer_dump     (vnodeVer*);

/*
 * for vnodeInfo
 */
#define VNODEINFO_MAX_ADDRS ((int16_t)12)
#define VNODEINFO_MIN_ADDRS ((int16_t)4)
struct vnodeInfo_relax {
    vnodeId  id;
    vnodeVer ver;
    int32_t  weight;

    int16_t  naddrs;
    int16_t  capc;
    struct sockaddr_in addrs[VNODEINFO_MAX_ADDRS];
};
typedef struct vnodeInfo_relax vnodeInfo_relax;

vnodeInfo_relax* vnodeInfo_relax_alloc(void);
void vnodeInfo_relax_free(vnodeInfo_relax*);
int  vnodeInfo_relax_init(vnodeInfo_relax*, vnodeId*, vnodeVer*, int);

struct vnodeInfo {
    vnodeId  id;
    vnodeVer ver;
    int32_t  weight;

    int16_t  naddrs;
    int16_t  capc;
    struct sockaddr_in addrs[VNODEINFO_MIN_ADDRS];
};
typedef struct vnodeInfo vnodeInfo;
vnodeInfo* vnodeInfo_alloc(void);
void vnodeInfo_free(vnodeInfo*);
int  vnodeInfo_init(vnodeInfo*, vnodeId*, vnodeVer*, int);

int  vnodeInfo_add_addr (vnodeInfo**, struct sockaddr_in*);
int  vnodeInfo_has_addr (vnodeInfo*, struct sockaddr_in*);
int  vnodeInfo_cat_addr (vnodeInfo*, vnodeInfo*);
int  vnodeInfo_update   (vnodeInfo*, vnodeInfo*);
int  vnodeInfo_copy     (vnodeInfo*, vnodeInfo*);
void vnodeInfo_dump     (vnodeInfo*);

/*
 * for vnodeConn
 */
enum {
    VNODECONN_WEIGHT_LOW   = ((int32_t)0),
    VNODECONN_WEIGHT_HIGHT = ((int32_t)10)
};

struct vnodeConn {
    int32_t weight;
    struct sockaddr_in local;
    struct sockaddr_in remote;
};
typedef struct vnodeConn vnodeConn;

void vnodeConn_set   (vnodeConn*, struct sockaddr_in*, struct sockaddr_in*);
void vnodeConn_adjust(vnodeConn*, vnodeConn*);

/*
 * for vsrvcId
 */
typedef struct vtoken vsrvcHash;
int vsrvcId_bucket(vtoken*);

/*
 * for vsrvcInfo
 */
#define VSRVCINFO_MAX_ADDRS ((int16_t)12)
#define VSRVCINFO_MIN_ADDRS ((int16_t)2)
struct vsrvcInfo_relax {
    vsrvcHash hash;
    vnodeId hostid;
    int32_t nice;

    int16_t naddrs;
    int16_t capc;
    struct sockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
};
typedef struct vsrvcInfo_relax vsrvcInfo_relax;
int vsrvcInfo_relax_init(vsrvcInfo_relax*, vsrvcHash*, vnodeId*, int);

struct vsrvcInfo {
    vsrvcHash hash;
    vnodeId hostid;
    int32_t nice;

    int16_t naddrs;
    int16_t capc;
    struct sockaddr_in addrs[VSRVCINFO_MIN_ADDRS];
};
typedef struct vsrvcInfo vsrvcInfo;
typedef void (*vsrvcInfo_number_addr_t) (vsrvcHash*, int, void*);
typedef void (*vsrvcInfo_iterate_addr_t)(vsrvcHash*, struct sockaddr_in*, int, void*);
vsrvcInfo* vsrvcInfo_alloc(void);
void vsrvcInfo_free(vsrvcInfo*);
int  vsrvcInfo_init(vsrvcInfo*, vtoken*, vsrvcHash*, int);

int  vsrvcInfo_add_addr(vsrvcInfo**, struct sockaddr_in*);
void vsrvcInfo_del_addr(vsrvcInfo*, struct sockaddr_in*);
int  vsrvcInfo_is_empty(vsrvcInfo*);

int  vsrvcInfo_copy(vsrvcInfo*, vsrvcInfo*);
void vsrvcInfo_dump(vsrvcInfo*);

#endif

