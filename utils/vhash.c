#include "vglobal.h"
#include "vhash.h"

static
int _aux_power_of_2(int u)
{
    int ret = 1;
    while (ret < u) {
        ret <<= 1;
    }
    return ret;
}

int vhash_init(struct vhash* hash, int capc, void* cookie, vhash_func_t hash_cb, vhash_cmp_t cmp_cb)
{
    vassert(hash);
    vassert(capc > 0);
    vassert(hash_cb);
    vassert(cmp_cb);

    hash->capc = _aux_power_of_2(capc);
    hash->used = 0;
    hash->mask = hash->capc - 1;
    hash->cookie = cookie;
    hash->hash_cb = hash_cb;
    hash->cmp_cb  = cmp_cb;
    hash->buckets = NULL;

    return 0;
}

void vhash_deinit(struct vhash* hash)
{
    vassert(hash);

    //todo;
    return ;
}

int vhash_size(struct vhash* hash)
{
    vassert(hash);
    //todo;
    return 0;
}

void* vhash_get(struct vhash* hash, void* key)
{
    vassert(hash);
    vassert(key);

    //todo;
    return NULL;
}

int vhash_add(struct vhash* hash, void* key, void* val)
{
    vassert(hash);
    vassert(key);
    vassert(val);

    //todo;
    return 0;
}

void* vhash_del(struct vhash* hash, void* key)
{
    vassert(hash);
    vassert(key);

    //todo;
    return NULL;
}

void* vhash_del_by_value(struct vhash* hash, void* val)
{
    vassert(hash);
    vassert(val);

    //todo;
    return NULL;
}

void vhash_iterate(struct vhash* hash, vhash_iterate_t cb, void* cookie)
{
    vassert(hash);
    vassert(cb);

    //todo;
    return ;
}

void vhash_zero(struct vhash* hash, vhash_zero_t cb, void* cookie)
{
    vassert(hash);
    vassert(cb);
    //todo;

    return ;
}

