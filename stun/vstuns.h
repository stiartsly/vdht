#ifndef __VSTUN_H__
#define __VSTUN_H__

#include "vsys.h"
#include "vcfg.h"
#include "vrpc.h"
#include "vhost.h"
#include "vmsger.h"
#include "vstun_proto.h"

struct vstuns;
struct vstuns_ops {
    int (*render)   (struct vstuns*);
    int (*unrender) (struct vstuns*);
    int (*parse_msg)(struct vstuns*, void*);
    int (*daemonize)(struct vstuns*);
    int (*stop)     (struct vstuns*);
};

struct vstuns {
    struct vthread daemon;
    int daemonized;
    int to_quit;

    struct sockaddr_in my_addr;

    struct vhost*    host;
    struct vhashgen* hashgen;
    struct vrpc      rpc;
    struct vmsger    msger;
    struct vwaiter   waiter;

    struct vstun_proto_ops* proto_ops;
    struct vstuns_ops* ops;
};

struct vstuns* vstuns_create(struct vhost*, struct vconfig*);
void vstuns_destroy(struct vstuns*);

#endif

