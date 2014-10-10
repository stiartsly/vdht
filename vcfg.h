#ifndef __VCFG_H__
#define __VCFG_H__

struct vcfg_ops {
    int (*get_int)(const char*, int*);
    int (*get_str)(const char*, char*, int);
};

#endif
