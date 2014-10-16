#ifndef __VRPC_H__
#define __VRPC_H__

#include <sys/types.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include "vmsger.h"

enum {
    VRPC_NUL,
    VRPC_RCV,
    VRPC_SND,
    VRPC_ERR,
    VRPC_METHOD_BUTT
};

enum {
    VRPC_UNIX,
    VRPC_UDP,
    VRPC_TCP,
    VRPC_SUDP,
    VRPC_STCP,
    VRPC_MODE_BUTT
};
#define vrpc_mode_ok(mode) ((mode >= VRPC_UNIX) && (mode < VRPC_MODE_BUTT))

/*
 * for rpc
 */
struct vrpc_base_ops {
    void* (*open)   (struct vsockaddr*);
    int   (*sndto)  (void*, struct vmsg_sys*);
    int   (*rcvfrom)(void*, struct vmsg_sys*);
    void  (*close)  (void*);
    int   (*getfd)  (void*);
};

struct vrpc;
struct vrpc_ops {
    int (*snd)  (struct vrpc*);
    int (*rcv)  (struct vrpc*);
    int (*err)  (struct vrpc*);
    int (*getId)(struct vrpc*);
    int (*dump) (struct vrpc*);
};

struct vrpc {
    int mode;
    struct vsockaddr addr;

    struct vmsg_sys* rcvm;
    struct vmsg_sys* sndm;
    struct vmsger*   msger;

    struct vrpc_ops* ops;           //upword   methods
    struct vrpc_base_ops* base_ops; //downword methods
    void* impl;

    int nrcvs;
    int nsnds;
    int nerrs;
    int rcv_sz;
    int snd_sz;
};

int  vrpc_init  (struct vrpc*, struct vmsger*, int, struct vsockaddr*);
void vrpc_deinit(struct vrpc*);

/*
 * for rpc_waiter.
 */
struct vwaiter;
struct vwaiter_ops {
    int (*add)   (struct vwaiter*, struct vrpc*);
    int (*remove)(struct vwaiter*, struct vrpc*);
    int (*select)(struct vwaiter*, struct vrpc**, int*);
    int (*dump)  (struct vwaiter*);
};

struct vwaiter {
    struct vlock  lock;
    struct varray rpcs;
    int reset;

    int maxfd;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;

    struct vwaiter_ops* ops;
};
int  vwaiter_init  (struct vwaiter*);
void vwaiter_deinit(struct vwaiter*);

#endif
