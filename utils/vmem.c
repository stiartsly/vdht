#include "vglobal.h"
#include "vmem.h"

#define CHUNK_MAGIC     ((uint32_t)0x87654312)

/*
 * @aux:
 * @obj_sz:
 * @first_capc
 */
int vmem_aux_init(struct vmem_aux* aux, int obj_sz, int first_capc)
{
    vassert(aux);

    retE((obj_sz < 0));
    retE((first_capc < 0));

    aux->capc   = 0;
    aux->used   = 0;
    aux->first  = (first_capc > 0) ? first_capc : VMEM_FIRST_CAPC;
    aux->obj_sz = obj_sz;
    vlist_init(&aux->zones);
    return 0;
}

/*
 * @aux:
 */
int _aux_extend(struct vmem_aux* aux)
{
    int usz = sizeof(struct vmem_chunk) + aux->obj_sz;
    struct vmem_zone* zone = NULL;
    void* cache  = NULL;
    void* chunks = NULL;
    int i    = 0;
    vassert(aux);
    vassert(aux->used >= aux->capc);

    cache  = malloc(aux->first * usz);
    chunks = malloc(aux->first * sizeof(void*));
    zone = (struct vmem_zone*)malloc(sizeof(*zone));
    if ((!cache) || (!chunks) || (!zone)) {
        vlogEv((1), elog_malloc);
        if (cache)  free(cache);
        if (chunks) free(chunks);
        if (zone)   free(zone);
        retE((1));
    }
    memset(cache,  0, aux->first * usz);
    memset(chunks, 0, aux->first * sizeof(void*));
    memset(zone,   0, sizeof(*zone));

    zone->chunks = chunks;
    zone->mem_cache = cache;
    vlist_init(&zone->list);
    vlist_add_tail(&aux->zones, &zone->list);

    for (i = 0; i < aux->first; i++) {
        zone->chunks[i] = (struct vmem_chunk*)(zone->mem_cache + usz * i);
        zone->chunks[i]->magic = CHUNK_MAGIC;
        zone->chunks[i]->taken = 0;
    }
    aux->capc += aux->first;
    return 0;
}

/*
 *@aux
 */
void* vmem_aux_alloc(struct vmem_aux* aux)
{
    struct vmem_zone* zone = NULL;
    struct vlist* node = NULL;
    int found = 0;
    int i = 0;
    vassert(aux);

    if (aux->used >= aux->capc) {
        retE_p((_aux_extend(aux) < 0));
    }

    __vlist_for_each(node, &aux->zones) {
        zone = vlist_entry(node, struct vmem_zone, list);
        for (i = 0; i < aux->first; i++) {
            if (!zone->chunks[i]->taken) {
                zone->chunks[i]->taken = 1;
                found = 1;
                break;
            }
        }
        if (found) {
            break;
        }
    }
    vassert(found);
    aux->used++;
    memset(zone->chunks[i]->obj, 0, aux->obj_sz);
    return zone->chunks[i]->obj;
}

/*
 * @aux:
 * @obj
 */
void vmem_aux_free(struct vmem_aux* aux, void* obj)
{
    struct vmem_chunk* chunk = NULL;
    int sz = sizeof(uint32_t) + sizeof(int32_t);
    vassert(aux);

    retE_v((!obj));

    chunk = (struct vmem_chunk*)(obj - sz);
    vassert((chunk->magic == CHUNK_MAGIC));
    vassert((chunk->taken = 1));

    chunk->taken = 0;
    aux->used--;
    return ;
}

/*
 * @aux
 */
void vmem_aux_deinit(struct vmem_aux* aux)
{
    struct vmem_zone* zone = NULL;
    struct vlist* node = NULL;
    vassert(aux);

    __vlist_for_each(node, &aux->zones) {
        zone = vlist_entry(node, struct vmem_zone, list);
        if (zone->mem_cache) {
            free(zone->mem_cache);
        }
        if (zone->chunks) {
            free(zone->chunks);
        }
    }
    return ;
}

