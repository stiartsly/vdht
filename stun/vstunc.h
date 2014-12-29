#ifndef __VSTUNC_H__
#define __VSTUNC_H__

#include "varray.h"
#include "vsys.h"
#include "vmsger.h"
#include "vstun_proto.h"

enum {
    NAT_NONE,                 // has public network address.
    NAT_FULL_CONE,            // full cone NAT type.
    NAT_RESTRICTED_CONE,      // restricted cone NAT type.
    NAT_PORT_RESTRICTED_CONE, // port restricted cone NAT type.
    NAT_SYMMETRIC,            // symmetric NAT type.
    NAT_FIREWALL_BLOCKED,     // fully blocked by firewall
    NAT_BUTT
};

enum {
    NAT_CHECK_STEP1      = ((uint32_t)0x00),
    NAT_CHECK_STEP2      = ((uint32_t)0x01),
    NAT_CHECK_STEP3      = ((uint32_t)0x02),
    NAT_CHECK_STEP3_NEXT = ((uint32_t)0x03),
    NAT_CHECK_STEP4      = ((uint32_t)0x04),
    NAT_CHECK_SUCC       = ((uint32_t)0x05),
    NAT_CHECK_ERR        = ((uint32_t)0x06),
    NAT_CHECK_TIMEOUT    = ((uint32_t)0x07),
    NAT_CHECK_BUTT       = ((uint32_t)0x08)
};

struct vstunc;
typedef int (*stunc_msg_cb_t)(void*, struct vstun_msg*);

struct vstunc_base_ops {
    int  (*add_srv)    (struct vstunc*, struct sockaddr_in*);
    int  (*snd_req_msg)(struct vstunc*, struct vstun_msg*, stunc_msg_cb_t, void*, int);
    int  (*rcv_rsp_msg)(struct vstunc*, struct vstun_msg*, struct sockaddr_in*);
    int  (*reap_tmo_reqs)(struct vstunc*);
    void (*clear)      (struct vstunc*);
    void (*dump)       (struct vstunc*);
};

typedef int (*get_ext_addr_cb_t)(void*, struct sockaddr_in*);
typedef int (*get_nat_type_cb_t)(void*, int);
struct vstunc_nat_ops {
    int  (*get_ext_addr)(struct vstunc*, struct sockaddr_in*, get_ext_addr_cb_t, void*);
    int  (*get_nat_type)(struct vstunc*, get_nat_type_cb_t, void*);
};

typedef int (*stunc_nat_check_t)(void*, stunc_msg_cb_t);
struct vstunc {
    struct varray items;   //stunc request records;
    struct varray servers;
    struct vlock  lock;

    int max_snd_tmo;
    int max_snd_tms; 

    struct vmsger*  msger;
    struct vticker* ticker;
    struct vroute*  route;
    void* private_params;

    struct vstunc_base_ops* base_ops;
    struct vstunc_nat_ops*  ops;
    stunc_nat_check_t* nat_ops;
    struct vstun_proto_ops* proto_ops;
};

int  vstunc_init(struct vstunc*, struct vmsger*, struct vticker*, struct vroute*);
void vstunc_deinit(struct vstunc*);

#endif

