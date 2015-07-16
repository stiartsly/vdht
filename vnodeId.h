#ifndef __VNODE_ID_H__
#define __VNODE_ID_H__

#include <netinet/in.h>

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

enum {
    VSOCKADDR_LOCAL,
    VSOCKADDR_UPNP,
    VSOCKADDR_REFLEXIVE,
    VSOCKADDR_RELAY,
    VSOCKADDR_UNKNOWN,
    VSOCKADDR_BUTT
};
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
int  vnodeInfo_is_fake  (vnodeInfo*);
void vnodeInfo_dump     (vnodeInfo*);
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
    struct vsockaddr_in addrs[VSRVCINFO_MAX_ADDRS];
};
typedef struct vsrvcInfo_relax vsrvcInfo_relax;
int vsrvcInfo_relax_init(vsrvcInfo_relax*, vsrvcHash*, vnodeId*, int);

struct vsrvcInfo {
    vsrvcHash hash;
    vnodeId hostid;
    int32_t nice;  // high 16 bits for protocol;
                   // low 16 bits for nice value;

    int16_t naddrs;
    int16_t capc;
    struct vsockaddr_in addrs[VSRVCINFO_MIN_ADDRS];
};

enum {
    VPROTO_RES0,
    VPROTO_UDP,
    VPROTO_TCP,
    VPROTO_RES1,
    VPROTO_RES2,
    VPROTO_UNKNOWN
};
typedef struct vsrvcInfo vsrvcInfo;
typedef void (*vsrvcInfo_number_addr_t) (vsrvcHash*, int, int, void*);
typedef void (*vsrvcInfo_iterate_addr_t)(vsrvcHash*, struct sockaddr_in*, uint32_t, int, void*);

static inline
int32_t vsrvcInfo_proto(vsrvcInfo* srvci) {
    return (srvci->nice >> 16);
}

static inline
int32_t vsrvcInfo_nice(vsrvcInfo* srvci) {
    return (srvci->nice & 0x0000ffff);
}

static inline
void vsrvcInfo_set_nice(vsrvcInfo* srvci, int32_t nice)
{
    srvci->nice &= 0xffff0000;
    srvci->nice |= 0x0000ffff & nice;
    return ;
}

vsrvcInfo* vsrvcInfo_alloc(void);
void vsrvcInfo_free(vsrvcInfo*);
int  vsrvcInfo_init(vsrvcInfo*, vtoken*, vsrvcHash*, int);

int  vsrvcInfo_add_addr(vsrvcInfo**, struct sockaddr_in*, uint32_t);
void vsrvcInfo_del_addr(vsrvcInfo*,  struct sockaddr_in*);
int  vsrvcInfo_is_empty(vsrvcInfo*);

int  vsrvcInfo_copy(vsrvcInfo*, vsrvcInfo*);
void vsrvcInfo_dump(vsrvcInfo*);

#define DECL_VSRVC_RELAX(srvci) \
    vsrvcInfo_relax srvci_relax = { \
        .hash   = {.data = {0}}, \
        .hostid = {.data = {0}}, \
        .nice   = 0, \
        .naddrs = 0, \
        .capc   = VSRVCINFO_MAX_ADDRS \
    }; \
    vsrvcInfo* srvci = (vsrvcInfo*)&srvci_relax;

#endif

