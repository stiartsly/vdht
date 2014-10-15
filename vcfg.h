#ifndef __VCFG_H__
#define __VCFG_H__

struct vconfig;
struct vconfig_ops {
    int (*parse)  (struct vconfig*, const char*);
    int (*clear)  (struct vconfig*);
    int (*get_int)(struct vconfig*, const char*, int*);
    int (*get_str)(struct vconfig*, const char*, char*, int);
};

struct vconfig {
    struct vlist items;
    struct vconfig_ops* ops;
};

int  vconfig_init  (struct vconfig*);
void vconfig_deinit(struct vconfig*);

#endif

