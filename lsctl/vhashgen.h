#ifndef __VHASHGEN_H__
#define __VHASHGEN_H__

#include "vdhtapi_inc.h"

struct vsrvcHash {
    uint8_t data[20];
};
typedef struct vsrvcHash vsrvcHash;


/*
 * use HASH-SHA1 algorithm
 */
struct vhashgen;
struct vhashgen_sha1_ops {
    void (*reset) (struct vhashgen*);
    void (*input) (struct vhashgen*, uint8_t*, int);
    int  (*result)(struct vhashgen*, uint32_t*);
};

struct vhashgen_ops {
    int (*hash)   (struct vhashgen*, uint8_t*, int, vsrvcHash*);
    int (*hash_with_cookie)(struct vhashgen*, uint8_t*, int, uint8_t*, int, vsrvcHash*);
};

struct vhashgen {
    void* ctxt;

    struct vhashgen_sha1_ops* sha_ops;
    struct vhashgen_ops*      ops;
};

int  vhashgen_init  (struct vhashgen*);
void vhashgen_deinit(struct vhashgen*);

#endif

