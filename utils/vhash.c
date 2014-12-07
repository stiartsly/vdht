#include "vglobal.h"
#include "vhash.h"

static MEM_AUX_INIT(hash_item_cache, sizeof(struct vmsg_sys), 8);
static
struct vhash_item* vhash_item_alloc(void)
{
    struct vhash_item* item = NULL;

    item = (struct vhash_item*)vmem_aux_alloc(&hash_item_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    memset(item, 0, sizeof(*item));
    return item;
}

static
void vhash_item_free(struct vhash_item* item)
{
    vassert(item);
    if (item->key) {
        free(item->key);
    }
    vmem_aux_free(&hash_item_cache, item);
    return ;
}

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

void vhash_deinit(struct vhash* hash, vhash_zero_t cb, void* cookie)
{
    struct vhash_bucket* bucket = NULL;
    struct vhash_item*   item   = NULL;
    struct vlist* node = NULL;
    vassert(hash);
    int i = 0;

    if (!hash->buckets) {
        return ;
    }

    for (; i < hash->capc; i++) {
        bucket = hash->buckets[i];
        while(!vlist_is_empty(&bucket->list)) {
            node = vlist_pop_head(&bucket->list);
            item = vlist_entry(node, struct vhash_item, list);
            cb(item->val, cookie);
            free(item->key);
            vhash_item_free(item);
        }
    }

    free(hash->buckets);
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
    struct vhash_item* item = NULL;
    struct vlist* node = NULL;
    void* val = NULL;
    int found = 0;
    int idx = 0;

    vassert(hash);
    vassert(key);

    if (!hash->buckets) {
        return NULL;
    }

    idx = hash->hash_cb(key, hash->cookie);
    retE_p((idx >= hash->capc));

    __vlist_for_each(node, &(hash->buckets[idx])->list) {
        item = vlist_entry(node, struct vhash_item, list);
        if (hash->cmp_cb(item->key, key, hash->cookie)) {
            found = 1;
            val = item->val;
            break;
        }
    }
    retE_p((!found));
    vlist_del(&item->list);
    vhash_item_free(item);
    return val;
}

void* vhash_del_by_value(struct vhash* hash, void* val)
{
    struct vhash_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;
    int i = 0;

    vassert(hash);
    vassert(val);

    if (!hash->buckets) {
        return NULL;
    }

    for (; i < hash->capc; i++) {
        __vlist_for_each(node, &(hash->buckets[i])->list) {
            item = vlist_entry(node, struct vhash_item, list);
            if (item->val == val) {
                found = 1;
                break;
            }
        }
    }
    retE_p((!found));

    vlist_del(&item->list);
    vhash_item_free(item);
    return val;
}

void vhash_iterate(struct vhash* hash, vhash_iterate_t cb, void* cookie)
{
    struct vhash_item*   item   = NULL;
    struct vlist* node = NULL;
    int i = 0;

    vassert(hash);
    vassert(cb);

    if (!hash->buckets) {
        return ;
    }
    for (; i < hash->capc; i++) {
        __vlist_for_each(node, &(hash->buckets[i])->list) {
            item = vlist_entry(node, struct vhash_item, list);
            if (cb(item->key, item->val, cookie) > 0) {
                return ;
            }
        }
    }
    return ;
}

