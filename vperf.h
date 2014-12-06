#ifndef __VPERF_H__
#define __VPERF_H__

struct vperf;
struct vperf_ops {
    int (*start)(struct vperf*);
    int (*stop) (struct vperf*);
    int (*enter)(struct vperf*);
    int (*leave)(struct vperf*);
    int (*dump) (struct vperf*);
};

struct vperf {
    int state;

    //todo;

    struct vperf_ops* ops;
};

int  vperf_init  (struct vperf*, struct vconfig*, struct vticker*);
void vperf_deinit(struct vperf*);

#endif

