#ifndef __VHASHGEN_H__
#define __VHASHGEN_H__

#ifndef uint8_t
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef signed short   int16_t;
typedef unsigned int   uint32_t;
typedef signed int     int32_t;
#endif

struct vsrvcHash {
    uint8_t data[20];
};
typedef struct vsrvcHash vsrvcHash;


#define HASH_MAGIC_RELAY  "@elastos.relay@none@kortide.org@relay@c60394519b248604e106db38a9f138d1eb75c28848b003527bf0d07603e40ec9e7a21a82c6a187a8a762e05053"
#define HASH_MAGIC_STUN   "@elastos.stun@none@kortide.org@stun@39d3a0eb5d501648ca5a087d1417a2637426e375d9a44cbec80680d2a909d9e2dd7acd3acae382943e8b1152a2b4"
struct vhashgen;

/*
 * use HASH-SHA1 algorithm
 */
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

int vhashhelper_get_stun_srvcHash (vsrvcHash*);
int vhashhelper_get_relay_srvcHash(vsrvcHash*);

#endif

