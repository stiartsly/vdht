#ifndef __VTICKER_H__
#define __VTICKER_H__

#include "vlist.h"
#include "vsys.h"

/*
 * for tick_cb
 */
typedef int (*vtick_t)(void*);
struct vtick_cb {
    struct vlist list;
    vtick_t cb;
    void* cookie;
};

struct vtick_cb* vtick_cb_alloc(void);
void vtick_cb_free(struct vtick_cb*);
void vtick_cb_init(struct vtick_cb*, vtick_t, void*);

/*
 * for vticker
 */
struct vticker;
struct vticker_ops {
    int (*add_cb) (struct vticker*, vtick_t, void*);
    int (*start)  (struct vticker*, int);
    int (*restart)(struct vticker*, int);
    int (*stop)   (struct vticker*);
    int (*clear)  (struct vticker*);
};

struct vticker {
    struct vtimer timer;

    struct vlist cbs;
    struct vlock lock;

    struct vticker_ops* ops;
};

int  vticker_init  (struct vticker*);
void vticker_deinit(struct vticker*);

#endif
