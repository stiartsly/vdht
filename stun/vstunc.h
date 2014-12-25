#ifndef __VSTUNC_H__
#define __VSTUNC_H__

#include "vlist.h"
#include "vsys.h"
#include "vmsger.h"

struct vstunc;
struct vstunc_nat_ops {
    int (*check_nat_blocked)(struct vstunc*, struct sockaddr_in*);
    int (*check_full_cone)  (struct vstunc*, struct sockaddr_in*);
    int (*check_symmetric)  (struct vstunc*, struct sockaddr_in*, struct sockaddr_in*);
    int (*check_port_restricted)(struct vstunc*, struct sockaddr_in*);
};

typedef int (*handle_mapped_addr_t)(struct sockaddr_in*, void*);
struct vstunc_ops {
    int (*add_server)     (struct vstunc*, char*, struct sockaddr_in*);
    int (*req_mapped_addr)(struct vstunc*, struct sockaddr_in*, handle_mapped_addr_t, void*);
    int (*rsp_mapped_addr)(struct vstunc*, struct sockaddr_in*, char*, int);
    int (*get_nat_type)   (struct vstunc*);
    int (*clear)          (struct vstunc*);
};

struct vstunc {
    struct vlist items;
    struct vlist servers;
    struct vlock lock;

    struct vmsger* msger;
    struct vstunc_ops* ops;
    struct vstun_proto_ops* proto_ops;
};


int  vstunc_init(struct vstunc*, struct vmsger*);
void vstunc_deinit(struct vstunc*);

#endif

