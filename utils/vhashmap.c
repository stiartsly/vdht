#include "vglobal.h"
#include "vhashmap.h"

static MEM_AUX_INIT(hashmap_item_cache, sizeof(struct vhashmap_item), 8);
static
struct vhashmap_item* vhashmap_item_alloc(void)
{
    struct vhashmap_item* item = NULL;

    item = (struct vhashmap_item*)vmem_aux_alloc(&hashmap_item_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    memset(item, 0, sizeof(*item));
    return item;
}

static
void vhashmap_item_free(struct vhashmap_item* item)
{
    vassert(item);
    vmem_aux_free(&hashmap_item_cache, item);
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

int vhashmap_init(struct vhashmap* hash, int capc, void* cookie,
        vhashmap_hash_t hash_cb,
        vhashmap_cmp_t cmp_cb,
        vhashmap_free_t free_cb)
{
    vassert(hash);
    vassert(capc > 0);
    vassert(hash_cb);
    vassert(cmp_cb);
    vassert(free_cb);

    hash->capc = _aux_power_of_2(capc);
    hash->mask = hash->capc - 1;
    hash->used = 0;
    hash->cookie = cookie;
    hash->hash_cb = hash_cb;
    hash->cmp_cb  = cmp_cb;
    hash->free_cb = free_cb;
    hash->buckets = NULL;

    return 0;
}

void vhashmap_deinit(struct vhashmap* hash)
{
    vassert(hash);
    retE_v((!hash->buckets));

    free(hash->buckets);
    return ;
}

int vhashmap_size(struct vhashmap* hash)
{
    vassert(hash);
    return hash->used;
}

void* vhashmap_get(struct vhashmap* hash, void* key)
{
    struct vhashmap_bucket* bucket = NULL;
    struct vhashmap_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;
    int hval = 0;

    vassert(hash);
    vassert(key);
    retE_p((!hash->buckets));

    hval = hash->hash_cb(key, hash->cookie) & hash->mask;
    bucket = hash->buckets[hval];
    __vlist_for_each(node, &bucket->list) {
        item = vlist_entry(node, struct vhashmap_item, list);
        if (hash->cmp_cb(key, item->val)) {
            found = 1;
            break;
        }
        item = NULL;
    }

    retE_p((!found));
    return item->val;
}

int vhashmap_add(struct vhashmap* hash, void* key, void* val)
{
    struct vhashmap_bucket* bucket = NULL;
    struct vhashmap_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;
    int hval = 0;

    vassert(hash);
    vassert(key);
    vassert(val);

    if (!hash->buckets) {
        hash->buckets = (struct vhashmap_bucket**)malloc(hash->capc * sizeof(struct vhashmap_bucket));
        vlog((!hash->buckets), elog_malloc);
        retE((!hash->buckets));
    }

    hval = hash->hash_cb(key, hash->cookie) & hash->mask;
    bucket = hash->buckets[hval];

    __vlist_for_each(node, &bucket->list) {
        item = vlist_entry(node, struct vhashmap_item, list);
        if (hash->cmp_cb(key, item->val)) {
            hash->free_cb(item->val);
            item->val = val;
            found = 1;
            break;
        }
    }
    if (!found) {
        item = vhashmap_item_alloc();
        //vlog((!item), elog_vhashmap_item_alloc);
        retE((!item));
        vlist_init(&item->list);
        item->val = val;
        vlist_add_tail(&bucket->list, &item->list);
        hash->used++;
    }
    return 0;
}

void* vhashmap_del(struct vhashmap* hash, void* key)
{
    struct vhashmap_bucket* bucket = NULL;
    struct vhashmap_item* item = NULL;
    struct vlist* node = NULL;
    void* val = NULL;
    int hval = 0;

    vassert(hash);
    vassert(key);
    retE_p((!hash->buckets));

    hval = hash->hash_cb(key, hash->cookie) & hash->mask;
    bucket = hash->buckets[hval];

    __vlist_for_each(node, &bucket->list) {
        item = vlist_entry(node, struct vhashmap_item, list);
        if (hash->cmp_cb(key, item->val)) {
            val = item->val;
            break;
        }
        item = NULL;
    }
    retE_p((!val));
    vlist_del(&item->list);
    vhashmap_item_free(item);
    hash->used--;

    return val;
}

void* vhashmap_del_by_value(struct vhashmap* hash, void* val)
{
    struct vhashmap_bucket* bucket = NULL;
    struct vhashmap_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;
    int i = 0;

    vassert(hash);
    vassert(val);
    retE_p((!hash->buckets));

    for (; i < hash->capc; i++) {
        bucket = hash->buckets[i];
        __vlist_for_each(node, &bucket->list) {
            item = vlist_entry(node, struct vhashmap_item, list);
            if (item->val == val) {
                found = 1;
                break;
            }
            item = NULL;
        }
    }
    retE_p((!found));

    vlist_del(&item->list);
    vhashmap_item_free(item);
    hash->used--;
    return val;
}

void vhashmap_zero(struct vhashmap* hash)
{
    struct vhashmap_bucket* bucket = NULL;
    struct vhashmap_item* item = NULL;
    struct vlist* node = NULL;
    int i = 0;

    vassert(hash);
    retE_v((!hash->buckets));

    for (; i < hash->capc; i++) {
        bucket = hash->buckets[i];
        while (!vlist_is_empty(&bucket->list)) {
            node = vlist_pop_head(&bucket->list);
            item = vlist_entry(node, struct vhashmap_item, list);
            hash->free_cb(item->val);
            vhashmap_item_free(item);
            hash->used--;
        }
    }
    return ;
}

void vhashmap_iterate(struct vhashmap* hash, vhashmap_iterate_t cb, void* cookie)
{
    struct vhashmap_bucket* bucket = NULL;
    struct vhashmap_item* item = NULL;
    struct vlist* node = NULL;
    int i = 0;

    vassert(hash);
    vassert(cb);
    retE_v((!hash->buckets));

    for (; i < hash->capc; i++) {
        bucket = hash->buckets[i];
        __vlist_for_each(node, &bucket->list) {
            item = vlist_entry(node, struct vhashmap_item, list);
            if (cb(item->val, cookie) > 0) {
                return ;
            }
        }
    }
    return ;
}

