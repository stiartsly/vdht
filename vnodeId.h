#ifndef __VNODE_ID_H__
#define __VNODE_ID_H__

#include <netinet/in.h>

typedef int(*vdump_t)(const char*, ...);

// used for message token.
#define VNONCE_LEN    4
#define VNONCE_INTLEN 1
#define VNONCE_BITLEN 32

struct vnonce {
    uint8_t data[VNONCE_LEN];
};
typedef struct vnonce vnonce;
void vnonce_make   (vnonce*);
int  vnonce_equal  (vnonce*, vnonce*);
void vnonce_copy   (vnonce*, vnonce*);
int  vnonce_strlize(vnonce*, char*, int);
int  vnonce_unstrlize(const char*, vnonce*);

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
void vtoken_dump   (vtoken*, vdump_t);
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
void vnodeVer_dump     (vnodeVer*, vdump_t );

#define VSOCKADDR_TYPE(type) (0x00ff & type)

struct vsockaddr_in {
    struct sockaddr_in addr;
    uint32_t type;
};

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
    struct vsockaddr_in addrs[VNODEINFO_MAX_ADDRS];
};
typedef struct vnodeInfo_relax vnodeInfo_relax;
struct vnodeInfo {
    vnodeId  id;
    vnodeVer ver;
    int32_t  weight;

    int16_t  naddrs;
    int16_t  capc;
    struct vsockaddr_in addrs[VNODEINFO_MIN_ADDRS];
};
typedef struct vnodeInfo vnodeInfo;
vnodeInfo* vnodeInfo_relax_alloc();
void vnodeInfo_relax_free(vnodeInfo*);
int  vnodeInfo_relax_init(vnodeInfo*, vnodeId*, vnodeVer*, int);

vnodeInfo* vnodeInfo_alloc(void);
void vnodeInfo_free(vnodeInfo*);
int  vnodeInfo_init(vnodeInfo*, vnodeId*, vnodeVer*, int);

int  vnodeInfo_add_addr (vnodeInfo**, struct sockaddr_in*, uint32_t);
int  vnodeInfo_has_addr (vnodeInfo*,  struct sockaddr_in*);
int  vnodeInfo_merge    (vnodeInfo**, vnodeInfo*);
int  vnodeInfo_copy     (vnodeInfo**, vnodeInfo*);
int  vnodeInfo_size     (vnodeInfo*);
int  vnodeInfo_is_fake  (vnodeInfo*);
void vnodeInfo_dump     (vnodeInfo*, vdump_t);
struct sockaddr_in* vnodeInfo_worst_addr(vnodeInfo*);

#define DECL_VNODE_RELAX(nodei) \
    vnodeInfo_relax nodei_relax = { \
        .id     = { .data = {0}}, \
        .ver    = { .data = {0}}, \
        .weight = 0, \
        .naddrs = 0, \
        .capc   = VNODEINFO_MAX_ADDRS \
    };\
    vnodeInfo* nodei = (vnodeInfo*)&nodei_relax;

/*
 * for vnodeConn
 */
enum {
    VNODECONN_WEIGHT_LOW   = ((int32_t)0),
    VNODECONN_WEIGHT_HIGHT = ((int32_t)10)
};

struct vnodeConn {
    int32_t priority;
    struct sockaddr_in laddr;
    struct sockaddr_in raddr;
};
typedef struct vnodeConn vnodeConn;

void vnodeConn_set    (vnodeConn*, struct vsockaddr_in*, struct vsockaddr_in*);
void vnodeConn_set_raw(vnodeConn*, struct sockaddr_in*,  struct sockaddr_in* );
void vnodeConn_adjust (vnodeConn*, vnodeConn*);

/*
 * for vsrvcHash
 */
int  vsrvcId_bucket   (vtoken*);
int  vsrvcHash_equal  (vsrvcHash*, vsrvcHash*);
void vsrvcHash_copy   (vsrvcHash*, vsrvcHash*);
int  vsrvcHash_strlize(vsrvcHash*, char*, int);

/*
 * for vsrvcInfo
 */
#define VSRVCINFO_MAX_ADDRS ((int16_t)12)
#define VSRVCINFO_MIN_ADDRS ((int16_t)2)
struct vsrvcInfo_relax {
    vsrvcHash hash;
    vnodeId   hostid;
    int16_t   nice; //index of CPU (,memory, network traffic, etc) resource avaiability.

    int16_t proto; //currently only support for UDP and TCP.
    int16_t naddrs;
    int16_t capc;
    struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
};

struct vsrvcInfo {
    vsrvcHash hash;
    vnodeId   hostid;
    int16_t   nice;

    int16_t proto;
    int16_t naddrs;
    int16_t capc;
    struct vsockaddr_in addrs[VSRVCINFO_MIN_ADDRS];
};
typedef struct vsrvcInfo_relax vsrvcInfo_relax;
typedef struct vsrvcInfo vsrvcInfo;

int vsrvcInfo_relax_init(vsrvcInfo*, vsrvcHash*, vnodeId*, int16_t, int16_t);

vsrvcInfo* vsrvcInfo_alloc(void);
void vsrvcInfo_free(vsrvcInfo*);
int  vsrvcInfo_init(vsrvcInfo*, vsrvcHash*, vnodeId*, int16_t, int16_t);

int  vsrvcInfo_add_addr(vsrvcInfo**, struct sockaddr_in*, uint32_t);
void vsrvcInfo_del_addr(vsrvcInfo*,  struct sockaddr_in*);
int  vsrvcInfo_is_empty(vsrvcInfo*);

int  vsrvcInfo_size(vsrvcInfo*);
int  vsrvcInfo_copy(vsrvcInfo**, vsrvcInfo*);
void vsrvcInfo_dump(vsrvcInfo*, vdump_t);

#define DECL_VSRVC_RELAX(srvci) \
    vsrvcInfo_relax srvci_relax = { \
        .hash   = {.data = {0}}, \
        .hostid = {.data = {0}}, \
        .nice   = 0, \
        .proto  = 0, \
        .naddrs = 0, \
        .capc   = VSRVCINFO_MAX_ADDRS \
    }; \
    vsrvcInfo* srvci = (vsrvcInfo*)&srvci_relax;

#endif

