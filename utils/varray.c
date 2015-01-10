#include "vglobal.h"
#include "varray.h"

/*
 *  for array
 * @array:
 * @first_capc:
 */
int varray_init(struct varray* array, int first_capc)
{
    vassert(array);

    array->capc  = 0;
    array->used  = 0;
    array->first = first_capc;
    array->items = NULL;
    if (!array->first) {
        array->first = VARRAY_FIRST_CAPC;
    }
    return 0;
}

void varray_deinit(struct varray* array)
{
    vassert(array);

    if (array->items) {
        free(array->items);
    }
    return ;
}

int varray_size(struct varray* array)
{
    vassert(array);
    return array->used;
}

void* varray_get(struct varray* array, int idx)
{
    vassert(array);

    retE_p((!array->items));
    retE_p((idx < 0));
    retE_p((idx >= array->used));

    return array->items[idx];
}

void* varray_get_rand(struct varray* array)
{
    int idx = 0;
    vassert(array);

    retE_p((!array->items));

    srand((int)time(NULL));
    idx = rand() % (array->used);
    return array->items[idx];
}

int varray_set(struct varray* array, int idx, void* item)
{
    vassert(array);

    retE((!item));
    retE((idx < 0));
    retE((idx >= array->used));
    retE((!array->items));

    array->items[idx] = item;
    return 0;
}

static
int _aux_extend(struct varray* array)
{
    void* new_items = NULL;
    int   new_capc  = 0;

    vassert(array);
    vassert(array->used >= array->capc);

    if (!array->capc) {
        new_capc = array->first;
    } else {
        new_capc = array->capc << 1;
    }

    new_items = realloc(array->items, new_capc * sizeof(void*));
    ret1E((!new_items), elog_realloc);

    array->capc  = new_capc;
    array->items = (void**)new_items;
    return 0;
}

int varray_add(struct varray* array, int idx, void* new)
{
    int i = array->used;
    vassert(array);

    retE((idx < 0));
    retE((idx >= array->used));
    retE((!new));

    if (array->used >= array->capc) {
        retE((_aux_extend(array) < 0));
    }
    for (; i >= idx; i--) {
        array->items[i+1] = array->items[i];
    }
    array->items[idx] = new;
    array->used++;
    return 0;
}

int varray_add_tail(struct varray* array, void* item)
{
    vassert(array);
    retE((!item));

    if (array->used >= array->capc) {
        retE((_aux_extend(array) < 0));
    }
    array->items[array->used++] = item;
    return 0;
}

static
int _aux_shrink(struct varray* array)
{
    void* new_items = NULL;
    int   new_capc  = array->capc >> 1;

    vassert(array);
    vassert(array->items);
    vassert(array->used * 2 <= array->capc);

    retS((array->capc == array->first));

    new_items = realloc(array->items, new_capc * sizeof(void*));
    ret1E((!new_items), elog_realloc);
    array->capc  = new_capc;
    array->items = (void**)new_items;
    return 0;
}

void* varray_del(struct varray* array, int idx)
{
    void* item = NULL;
    int i = 0;
    vassert(array);

    retE_p((idx < 0));
    retE_p((idx >= array->used));

    if ((array->capc >= array->first)
        && (array->capc > array->used * 2)) {
        retE_p(_aux_shrink(array) < 0);
    }

    item = array->items[idx];
    for (i = idx; i < array->used; i++) {
        array->items[i] = array->items[i+1];
    }
    array->items[i] = NULL;
    array->used--;
    return item;
}

void* varray_pop_tail(struct varray* array)
{
    void* item = NULL;
    vassert(array);

    if ((array->capc >= array->first)
        && (array->capc > array->used * 2)) {
        retE_p(_aux_shrink(array) < 0);
    }
    item = array->items[--array->used];
    array->items[array->used] = NULL;
    return item;
}

void varray_iterate(struct varray* array, varray_iterate_t cb, void* cookie)
{
    int i = 0;
    vassert(array);

    retE_v((!cb));
    for (; i < array->used; i++) {
        if (cb(array->items[i], cookie) > 0) {
            break;
        }
    }
    return ;
}

void varray_zero(struct varray* array, varray_zero_t zero_cb, void* cookie)
{
    vassert(array);
    retE_v((!zero_cb));

    while(varray_size(array) > 0) {
        void* item = varray_pop_tail(array);
        zero_cb(item, cookie);
    }
    return;
}

/*
 * for sorted array.
 */
int vsorted_array_init(struct vsorted_array* sarray, int first_capc, varray_cmp_t cb, void* cookie)
{
    vassert(sarray);

    retE((!cb));
    varray_init(&sarray->array, first_capc);
    sarray->cmp_cb = cb;
    sarray->cookie = cookie;
    return 0;
}

void vsorted_array_deinit(struct vsorted_array* sarray)
{
    vassert(sarray);
    varray_deinit(&sarray->array);
    return ;
}

int vsorted_array_size(struct vsorted_array* sarray)
{
    vassert(sarray);
    return varray_size(&sarray->array);
}

void* vsorted_array_get(struct vsorted_array* sarray, int idx)
{
    vassert(sarray);
    return varray_get(&sarray->array, idx);
}

int vsorted_array_add(struct vsorted_array* sarray, void* new)
{
    void* item = NULL;
    int sz = varray_size(&sarray->array);
    int i  = 0;

    vassert(sarray);
    retE((!new));

    for (; i < sz; i++) {
        item = varray_get(&sarray->array, i);
        if (sarray->cmp_cb(new, item, sarray->cookie) > 0) {
            // "new" is better than "item"
            break;
        }
    }
    if (i < sz) {
        return varray_add(&sarray->array, i, new);
    } else {
        return varray_add_tail(&sarray->array, new);
    }
    return 0;
}

void* vsorted_array_del(struct vsorted_array* sarray, void* todel)
{
    void* item = NULL;
    int sz = varray_size(&sarray->array);
    int i  = 0;

    vassert(sarray);
    retE_p((!todel));

    for (; i < sz; i++) {
       	item = varray_get(&sarray->array, i);
        if (sarray->cmp_cb(todel, item, &sarray->cookie) == 0) {
            break;
        }
    }
    retE_p((i >= sz));
    return varray_del(&sarray->array, i);
}


void vsorted_array_iterate(struct vsorted_array* sarray, varray_iterate_t cb, void* cookie)
{
    vassert(sarray);
    retE_v((!cb));

    varray_iterate(&sarray->array, cb, cookie);
    return ;
}

void vsorted_array_zero(struct vsorted_array* sarray, varray_zero_t zero_cb, void* cookie)
{
    vassert(sarray);
    retE_v((!zero_cb));

    varray_zero(&sarray->array, zero_cb, cookie);
    return ;
}

