#ifndef __VARRAY_H__
#define __VARRAY_H__

#define VARRAY_FIRST_CAPC (32)

/*
 * for array.
 */
typedef int (*varray_iterate_t)(void*, void*);
typedef void (*varray_zero_t)(void*, void*);

struct varray {
    int used;
    int capc;
    int first;
    void** items;
};

int   varray_init    (struct varray*, int);
void  varray_deinit  (struct varray*);
int   varray_size    (struct varray*);
void* varray_get     (struct varray*, int);
void* varray_get_rand(struct varray*);
int   varray_set     (struct varray*, int, void*);
int   varray_add     (struct varray*, int, void*);
int   varray_add_tail(struct varray*, void*);
void* varray_del     (struct varray*, int);
void* varray_pop_tail(struct varray*);
void  varray_iterate (struct varray*, varray_iterate_t, void*);
void  varray_zero    (struct varray*, varray_zero_t, void*);

/*
 * for sorted array
 */
typedef int (*varray_cmp_t)(void*, void*, void*);  /* >  0: former better than later.
                                              == 0: equally same.
                                              <  0: former worse than later.
                                            */

struct vsorted_array {
    struct varray array;
    varray_cmp_t cmp_cb;
    void* cookie;
};

int   vsorted_array_init   (struct vsorted_array*, int, varray_cmp_t, void*);
void  vsorted_array_deinit (struct vsorted_array*);
int   vsorted_array_size   (struct vsorted_array*);
void* vsorted_array_get    (struct vsorted_array*, int);
int   vsorted_array_add    (struct vsorted_array*, void*);
void* vsorted_array_del    (struct vsorted_array*, void*);
void  vsorted_array_iterate(struct vsorted_array*, varray_iterate_t, void*);
void  vsorted_array_zero   (struct vsorted_array*, varray_zero_t, void*);

#endif

