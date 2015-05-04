#ifndef __VMEM_H__
#define __VMEM_H__

#define VMEM_FIRST_CAPC ((int)4)

#include "vsys.h"

struct vmem_chunk {
    uint32_t magic;
    int32_t  taken;
    char obj[4];
};

struct vmem_zone {
    struct vlist list;
    struct vmem_chunk** chunks;
    void* mem_cache;
};

struct vmem_aux {
    int obj_sz;
    int used;
    int capc;
    int first;
    struct vlock lock;
    struct vlist zones;
};

#define MEM_AUX_INIT(maux, obj_sz, first_capc) \
    struct vmem_aux maux = { \
        obj_sz, \
        0, \
        0, \
        ((!first_capc) ? VMEM_FIRST_CAPC : first_capc ), \
        VLOCK_INITIALIZER, \
        {&maux.zones, &maux.zones }\
    }

int   vmem_aux_init  (struct vmem_aux*, int, int);
void* vmem_aux_alloc (struct vmem_aux*);
void  vmem_aux_free  (struct vmem_aux*, void*);
void  vmem_aux_deinit(struct vmem_aux*);

#endif

