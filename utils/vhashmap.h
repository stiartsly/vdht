#ifndef __VHASHMAP_H__
#define __VHASHMAP_H__

#include "vlist.h"

struct vhashmap_item {
    struct vlist list;
    void* val;
};

struct vhashmap_bucket {
    struct vlist list;
};

typedef int (*vhashmap_hash_t)(void*, void*);
typedef int (*vhashmap_cmp_t) (void*, void*);
typedef int (*vhashmap_free_t)(void*);

struct vhashmap {
    int capc;
    int mask;
    int used;
    void* cookie;

    vhashmap_hash_t hash_cb;
    vhashmap_cmp_t  cmp_cb;
    vhashmap_free_t free_cb;

    struct vhashmap_bucket** buckets;
};

typedef int (*vhashmap_iterate_t)(void*, void*);

int   vhashmap_init   (struct vhashmap*, int, void*, vhashmap_hash_t, vhashmap_cmp_t, vhashmap_free_t);
void  vhashmap_deinit (struct vhashmap*);

int   vhashmap_size   (struct vhashmap*);
void* vhashmap_get    (struct vhashmap*, void*);
int   vhashmap_add    (struct vhashmap*, void*, void*);
void* vhashmap_del    (struct vhashmap*, void*);
void* vhashmap_del_by_value(struct vhashmap*, void*);
void  vhashmap_zero   (struct vhashmap*);
void  vhashmap_iterate(struct vhashmap*, vhashmap_iterate_t, void*);

#endif

