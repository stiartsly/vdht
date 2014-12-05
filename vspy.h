#ifndef __VCOLLECT_H__
#define __VCOLLECT_H__

struct vspy;
struct vspy_ops {
    int (*get_nice)(struct vspy*, int*);
};

struct vspy {
    int cpu_ratio;
    int cpu_criteria;
    int cpu_factor;

    int mem_ratio;
    int mem_criteria;
    int mem_factor;

    int io_ratio;
    int io_criteria;
    int io_factor;

    int up_ratio;  // for upload-network
    int up_criteria;
    int up_factor;

    int down_ratio; // for download-network
    int down_criteria;
    int down_factor;

    struct vspy_ops* ops;
};

int  vspy_init  (struct vspy*, struct vconfig*);
void vspy_deinit(struct vspy*);

#endif


