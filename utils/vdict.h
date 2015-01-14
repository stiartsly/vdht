#ifndef __VDICTIONARY_H__
#define __VDICTIONARY_H__

#include "vlist.h"

struct vdict {
    int used;
    struct vlist items;
};

typedef int  (*vdict_iterate_t)(char*, void*, void*);
typedef void (*vdict_zero_t)(void*, void*);

int   vdict_init   (struct vdict*);
void  vdict_deinit (struct vdict*);
void* vdict_get    (struct vdict*, char*);
int   vdict_add    (struct vdict*, char*, void*);
void* vdict_del    (struct vdict*, char*);
void* vdict_del_by_val(struct vdict*, void*);
void  vdict_iterate(struct vdict*, vdict_iterate_t, void*);
void  vdict_zero   (struct vdict*, vdict_zero_t, void*);

struct vdict* vdict_alloc(void);
void vdict_free(struct vdict*);

#endif

