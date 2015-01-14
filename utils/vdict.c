#include "vglobal.h"
#include "vdict.h"

struct vdict_item {
    struct vlist list;
    char* key;
    void* val;
};

static MEM_AUX_INIT(dict_item_cache, sizeof(struct vdict_item), 8);
static
struct vdict_item* vdict_item_alloc(char* key)
{
    struct vdict_item* item = NULL;
    char* dup_key = NULL;
    vassert(key);

    dup_key = strdup(key);
    vlog((!dup_key), elog_strdup);
    retE_p((!dup_key));

    item = (struct vdict_item*)vmem_aux_alloc(&dict_item_cache);
    vlog((!item), elog_vmem_aux_alloc);
    ret1E_p((!item), free(dup_key));

    memset(item, 0, sizeof(*item));
    vlist_init(&item->list);
    item->key = dup_key;
    return item;
}

static
void vdict_item_free(struct vdict_item* item)
{
    if (item->key) {
        free(item->key);
    }
    vmem_aux_free(&dict_item_cache, item);
    return ;
}

int vdict_init(struct vdict* dict)
{
    vassert(dict);

    dict->used = 0;
    vlist_init(&dict->items);

    return 0;
}

void vdict_deinit(struct vdict* dict)
{
    vassert(dict);
    // do nothing;
    return;
}

int vdict_size(struct vdict* dict)
{
    vassert(dict);
    return dict->used;
}

void* vdict_get(struct vdict* dict, char* key)
{
    struct vdict_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(dict);
    vassert(key);

    __vlist_for_each(node, &dict->items) {
        item = vlist_entry(node, struct vdict_item, list);
        if (!strcmp(item->key, key)) {
            found = 1;
            break;
        }
    }
    return (found ? item->val : NULL);
}

int vdict_add(struct vdict* dict, char* key, void* val)
{
    struct vdict_item* item = NULL;

    vassert(dict);
    vassert(key);
    vassert(val);

    item = vdict_item_alloc(key);
    retE((!item));
    item->val = val;

    vlist_add_tail(&dict->items, &item->list);
    dict->used++;
    return 0;
}

void* vdict_del(struct vdict* dict, char* key)
{
    struct vdict_item* item = NULL;
    struct vlist* node = NULL;
    void* val = NULL;
    int found = 0;

    vassert(dict);
    vassert(key);

    __vlist_for_each(node, &dict->items) {
        item = vlist_entry(node, struct vdict_item, list);
        if (!strcmp(item->key, key)) {
            found = 1;
            break;
        }
        item = NULL;
    }

    if (found) {
        vlist_del(&item->list);
        val = item->val;
        item->val = NULL;
        vdict_item_free(item);
        dict->used--;
    }
    return val;
}

void* vdict_del_by_val(struct vdict* dict, void* val)
{
    struct vdict_item* item = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(dict);
    vassert(val);

    __vlist_for_each(node, &dict->items) {
        item = vlist_entry(node, struct vdict_item, list);
        if (item->val == val) {
            found = 1;
            break;
        }
        item = NULL;
    }
    if (found) {
        vlist_del(&item->list);
        item->val = NULL;
        vdict_item_free(item);
        dict->used--;
    }
    return (found ? val: NULL);
}

void vdict_iterate(struct vdict* dict, vdict_iterate_t cb, void* cookie)
{
    struct vdict_item* item= NULL;
    struct vlist* node = NULL;

    vassert(dict);
    vassert(cb);

    __vlist_for_each(node, &dict->items) {
        item = vlist_entry(node, struct vdict_item, list);
        if (cb(item->key, item->val, cookie) > 0) {
            break;
        }
    }
    return ;
}

void vdict_zero(struct vdict* dict, vdict_zero_t cb, void* cookie)
{
    struct vdict_item* item = NULL;
    struct vlist* node = NULL;

    vassert(dict);
    vassert(cb);

    while(!vlist_is_empty(&dict->items)) {
        node = vlist_pop_head(&dict->items);
        item = vlist_entry(node, struct vdict_item, list);

        cb(item, cookie);
        vdict_item_free(item);
        dict->used--;
    }
    return ;
}

