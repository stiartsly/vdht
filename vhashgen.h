#ifndef __VHASHGEN_H__
#define __VHASHGEN_H__

#include "vnodeId.h"

#define HASH_MAGIC_RELAY  "elastos.dht.service.relay.3eb2c2beb25d3ffeb4f5ce3dbcf618862baf69e8"
#define HASH_MAGIC_STUN   "elastos.dht.service.stunx.6e89fe134a90afb03da00123d3d0x01213edc3a0"

struct vhashgen;
struct vhashgen_ops {
    int (*hash)     (struct vhashgen*, char*, vtoken*);
    int (*hash_ext) (struct vhashgen*, char*, char*, vtoken*);
    int (*hash_frag)(struct vhashgen*, char*, int, vtoken*);
};

struct vhashgen {
    char* cookie;
    struct vhashgen_ops* ops;
};

int  vhashgen_init  (struct vhashgen*);
void vhashgen_deinit(struct vhashgen*);

#endif

