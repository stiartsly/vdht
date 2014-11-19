#ifndef __VPLUG_H__
#define __VPLUG_H__

#include "vlist.h"
#include "vnodeId.h"
#include "vroute.h"

#define VPLUG_MAGIC ((uint32_t)0x98431f031)
#define IS_PLUG_MSG(val) ((uint32_t)val == (uint32_t)VPLUG_MAGIC)

enum {
    PLUGIN_RELAY,
    PLUGIN_STUN,
    PLUGIN_VPN,
    PLUGIN_DDNS,
    PLUGIN_MROUTE,
    PLUGIN_DHASH,
    PLUGIN_APP,
    PLUGIN_BUTT
};

struct vpluger;
struct vpluger_ops {
    int  (*reg_service)  (struct vpluger*, int, struct sockaddr_in*);
    int  (*unreg_service)(struct vpluger*, int, struct sockaddr_in*);
    int  (*get_service)  (struct vpluger*, int, struct sockaddr_in*);
    int  (*clear)        (struct vpluger*);
    void (*dump)         (struct vpluger*);
};

/*
 * for plug manager
 */
struct vpluger {
    struct vlist services;  // for registred plugs;
    struct vlock lock;

    struct vroute* route;
    struct vmsger* msger;
    struct vpluger_ops* ops;
};

char* vpluger_get_desc(int);
int   vpluger_init  (struct vpluger*, struct vroute*);
void  vpluger_deinit(struct vpluger*);

#endif

