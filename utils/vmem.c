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
    retE((first_capc <= 0));

    aux->capc   = 0;
    aux->used   = 0;
    aux->first  = first_capc;
    aux->obj_sz = obj_sz;

    if (!aux->first) {
        aux->first = VMEM_FIRST_CAPC;
    }
    return 0;
}

/*
 * @aux:
 */
int _aux_extend(struct vmem_aux* aux)
{
    int usz = sizeof(struct vmem_chunk) + aux->obj_sz;
    void* cache  = NULL;
    void* chunks = NULL;
    int i  = 0;
    vassert(aux);
    vassert(aux->used >= aux->capc);

    if (!aux->capc) {
        aux->capc = aux->first;
    } else {
        aux->capc <<= 1;
    }

    if (aux->mem_cache) {
        cache  = malloc(aux->capc* usz);
        chunks = malloc(aux->capc* sizeof(void*));
    } else {
        cache  = realloc(aux->mem_cache, aux->capc* usz);
        chunks = realloc(aux->chunks, aux->capc* sizeof(void*));
    }
    if (!cache || !chunks) {
        if (cache ) free(cache);
        if (chunks) free(chunks);
        return -1;
    }
    aux->mem_cache = cache;
    aux->chunks    = chunks;

    for (; i < aux->capc; i++) {
        aux->chunks[i] = (struct vmem_chunk*)(aux->mem_cache + usz * i);
        aux->chunks[i]->magic = CHUNK_MAGIC;
        aux->chunks[i]->taken = 0;
    }
    return 0;
}

/*
 *@aux
 */
void* vmem_aux_alloc(struct vmem_aux* aux)
{
    int i = 0;
    vassert(aux);

    if (aux->used >= aux->capc) {
        retE_p((_aux_extend(aux) < 0));
    }
    for (; i < aux->capc; i++) {
        if (!aux->chunks[i]->taken) {
            break;
        }
    }
    retE_p((i >= aux->capc));

    aux->chunks[i]->taken = 1;
    aux->used++;
    return aux->chunks[i]->obj;
}

/*
 * @aux:
 * @obj
 */
void vmem_aux_free(struct vmem_aux* aux, void* obj)
{
    struct vmem_chunk* chunk = NULL;
    vassert(aux);

    retE_v((!obj));

    chunk = (struct vmem_chunk*)(obj - 8);
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
    vassert(aux);

    if (aux->mem_cache) {
        free(aux->mem_cache);
    }
    if (aux->chunks) {
        free(aux->chunks);
    }
    return ;
}

