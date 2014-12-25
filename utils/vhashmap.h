#ifndef __VHASHMAP_H__
#define __VHASHMAP_H__

#include "vlist.h"

struct vhash_item {
    struct vlist list;
    void* key;
    void* val;
    int   hash_val;
};

struct vhash_bucket {
    struct vlist list;
};

typedef int (*vhash_func_t)(void*, void*);
typedef int (*vhash_cmp_t) (void*, void*, void*);

struct vhash {
    int capc;
    int used;
    int mask;
    void* cookie;

    vhash_func_t hash_cb;
    vhash_cmp_t  cmp_cb;

    struct vhash_bucket** buckets;
};

typedef int (*vhash_iterate_t)(void*, void*, void*);
typedef int (*vhash_zero_t)   (void*, void*);

int   vhash_init   (struct vhash*, int, void*, vhash_func_t, vhash_cmp_t);
void  vhash_deinit (struct vhash*, vhash_zero_t, void*);

void* vhash_get    (struct vhash*, void*);
int   vhash_add    (struct vhash*, void*, void*);
int   vhash_size   (struct vhash*);
void* vhash_del    (struct vhash*, void*);
void* vhash_del_by_val(struct vhash*, void*);
void  vhash_iterate(struct vhash*, vhash_iterate_t, void*);

#endif
