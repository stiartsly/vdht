#ifndef __VHASHGEN_H__
#define __VHASHGEN_H__

#include "vnodeId.h"

#define HASH_MAGIC_RELAY  "elastos.dht.service.relay.3eb2c2beb25d3ffeb4f5ce3dbcf618862baf69e8"
#define HASH_MAGIC_STUN   "elastos.dht.service.stunx.6e89fe134a90afb03da00123d3d0x01213edc3a0"

struct vhashgen;

/*
 * use HASH-SHA1 algorithm
 */
struct vhashgen_sha1_ops {
    void (*reset)   (struct vhashgen*);
    void (*input)   (struct vhashgen*, uint8_t*, int);
    int  (*result)  (struct vhashgen*, uint32_t*);
};

struct vhashgen_ops {
    int (*hash)     (struct vhashgen*, uint8_t*, vtoken*);
    int (*hash_ext) (struct vhashgen*, uint8_t*, uint8_t*, vtoken*);
    int (*hash_frag)(struct vhashgen*, uint8_t*, int, vtoken*);
};

struct vhashgen_ctxt_sha1 {
    uint32_t digest[5];
    int len_low;
    int len_high;        // message length in bits.

    uint8_t msgblk[64]; // 512-bit message blocks;
    int msgblk_idx;     // index to message blocks;

    int computed;
    int corrupted;

    int oops;
    int done;
};

struct vhashgen {
    struct vhashgen_ctxt_sha1 ctxt_sha1;

    struct vhashgen_sha1_ops* sha_ops;
    struct vhashgen_ops*      ops;
};

int  vhashgen_init  (struct vhashgen*);
void vhashgen_deinit(struct vhashgen*);

#endif

