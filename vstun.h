#ifndef __VSTUNC_H__
#define __VSTUNC_H__

#include "vsys.h"
#include "vlist.h"
#include "vmsger.h"
#include "vhashgen.h"
#include "vstun_proto.h"

struct vstun;
typedef int (*get_ext_addr_t)(struct sockaddr_in*, void*);
struct vstun_base_ops {
    int  (*snd_req_msg) (struct vstun*, struct vstun_msg*, struct sockaddr_in*, get_ext_addr_t, void*);
    int  (*rcv_req_msg) (struct vstun*, struct vstun_msg*, struct sockaddr_in*);
    int  (*rcv_rsp_msg) (struct vstun*, struct vstun_msg*, struct sockaddr_in*);
    void (*reap_timeout_reqs)(struct vstun*);
    void (*clear)       (struct vstun*);
    int  (*load_servers)(struct vstun*);
};

struct vstun_ops {
    int  (*get_ext_addr)(struct vstun*, get_ext_addr_t, void*);
};

struct vstun {
    struct vlist  reqs;
    struct vlock  lock;

    int max_tmo;

    struct vmsger*   msger;
    struct vroute*   route;
    struct vhashgen* hashgen;

    struct vstun_ops *ops;
    struct vstun_base_ops *base_ops;
    struct vstun_proto_ops *proto_ops;

};

int  vstun_init  (struct vstun*, struct vmsger*, struct vroute*, struct vhashgen* hashgen);
void vstun_deinit(struct vstun*);

#endif

