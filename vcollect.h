#ifndef __VCOLLECT_H__
#define __VCOLLECT_H__

struct vcollect;
struct vcollect_ops {
    int (*collect_used_ratio)(struct vcollect*);
    int (*get_used_index)    (struct vcollect*, int*);
};

struct vcollect {
    int cpu_ratio;
    int cpu_criteria;
    int cpu_factor;

    int mem_ratio;
    int mem_criteria;
    int mem_factor;

    int io_ratio;
    int io_criteria;
    int io_factor;

    int up_ratio;
    int up_criteria;
    int up_factor;

    int down_ratio;
    int down_criteria;
    int down_factor;

    struct vcollect_ops* ops;
};

int  vcollect_init  (struct vcollect*, struct vconfig*);
void vcollect_deinit(struct vcollect*);

#endif


