#ifndef __VPLUG_H__
#define __VPLUG_H__

#include "vlist.h"
#include "vnodeId.h"
#include "vroute.h"

#define VPLUG_MAGIC ((uint32_t)0x98431f031)
#define IS_PLUG_MSG(val) ((uint32_t)val == (uint32_t)VPLUG_MAGIC)

struct vplug_desc {
    int   plugId;
    char* desc;
};

/*
 * for plug_c
 */
typedef int (*get_addr_cb_t)(struct sockaddr_in*, void*);
struct vplug_req {
    struct vlist list;
    vtoken token;
    int plugId;

    get_addr_cb_t cb;
    void* cookie;
};

struct vpluger;
struct vpluger_c_ops {
    int (*get_addr)(struct vpluger*, int, get_addr_cb_t,void*);
    int (*clear)   (struct vpluger*);
    int (*dump)    (struct vpluger*);
};

/*
 * for plug_s
 */
struct vplug_item {
    struct vlist list;
    char* desc;
    int  plugId;
    struct sockaddr_in addr;
};

struct vpluger_s_ops {
    int (*plug)  (struct vpluger*, int, struct sockaddr_in*);
    int (*unplug)(struct vpluger*, int, struct sockaddr_in*);
    int (*get)   (struct vpluger*, int, struct sockaddr_in*);
    int (*clear) (struct vpluger*);
    int (*dump)  (struct vpluger*);
};

/*
 * for plug manager
 */
struct vpluger {
    struct vlist plugs;  // for registered plugs.
    struct vlock plug_lock;

    struct vlist prqs;
    struct vlock prq_lock;

    struct vroute* route;
    struct vmsger* msger;

    struct vpluger_c_ops* c_ops;
    struct vpluger_s_ops* s_ops;
};

int  vpluger_init  (struct vpluger*, struct vhost*);
void vpluger_deinit(struct vpluger*);

#endif

