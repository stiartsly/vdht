#ifndef __VMSGER_H__
#define __VMSGER_H__

#include "vlist.h"
#include "vsys.h"

enum {
    VMSG_RSRVD = 0,
    VMSG_DHT   = 0x01,

    VMSG_LSCTL = 0x45,

    VMSG_BUTT
};

#define VDHT_MSG(msgId)  (msgId == VMSG_DHT)

/*
 * for message buffer
 */
void* vmsg_buf_alloc(int);
void  vmsg_buf_free(void*);

/*
 * vmsg_usr
 */
struct vsockaddr {
    union {
        struct sockaddr_in s_in;
        struct sockaddr_un s_un;
    }u_addr;
#define vsin_addr u_addr.s_in
#define vsun_addr u_addr.s_un
};

#define to_sockaddr_sin(addr)   ((struct sockaddr_in*)(&(addr)->vsin_addr))
#define to_sockaddr_sun(addr)   ((struct sockaddr_un*)(&(addr)->vsun_addr))

#define to_vsockaddr_from_sin(sin_addr) ((struct vsockaddr*)(sin_addr))
#define to_vsockaddr_from_sun(sun_addr) ((struct vsockaddr*)(sun_addr))

struct vmsg_usr {
    struct vsockaddr* addr;
    struct vsockaddr* spec;
    int   msgId;
    int   len;
    void* data;
};

void vmsg_usr_init(struct vmsg_usr*, int, struct vsockaddr*, struct vsockaddr*, int, void*);

/*
 * vmsg_sys
 */
struct vmsg_sys {
    struct vlist list;
    struct vsockaddr addr;
    struct vsockaddr spec;
    int   len;
    void* data;
};

struct vmsg_sys* vmsg_sys_alloc(int);
void vmsg_sys_free   (struct vmsg_sys*);
void vmsg_sys_init   (struct vmsg_sys*, struct vsockaddr*, struct vsockaddr*, int, void*);
void vmsg_sys_refresh(struct vmsg_sys*, int);

/*
 * for vmsg callback
 */
typedef int (*vmsg_cb_t)(void*, struct vmsg_usr*);
struct vmsg_cb {
    struct vlist list;
    int   msgId;
    void* cookie;
    vmsg_cb_t cb;
};
struct vmsg_cb* vmsg_cb_alloc(void);
void  vmsg_cb_free(struct vmsg_cb*);
void  vmsg_cb_init(struct vmsg_cb*, int, vmsg_cb_t, void*);


/*
 * for vmsger
 */
struct vmsger;
struct vmsger_ops {
    int (*dsptch) (struct vmsger*, struct vmsg_sys*);
    int (*add_cb) (struct vmsger*, void*, vmsg_cb_t, int);
    int (*push)   (struct vmsger*, struct vmsg_usr* );
    int (*popable)(struct vmsger*);
    int (*pop )   (struct vmsger*, struct vmsg_sys**);
    int (*clear)  (struct vmsger*);
    int (*dump)   (struct vmsger*);
};

typedef int (*vmsger_pack_t  )(void*, struct vmsg_usr*, struct vmsg_sys*);
typedef int (*vmsger_unpack_t)(void*, struct vmsg_sys*, struct vmsg_usr*);

struct vmsger {
    struct vlist  cbs;
    struct vlock  lock_cbs;
    struct vlist  msgs;
    struct vlock  lock_msgs;

    vmsger_pack_t pack_cb;
    void* cookie1;
    vmsger_unpack_t unpack_cb;
    void* cookie2;

    struct vmsger_ops* ops;
};

int  vmsger_init  (struct vmsger*);
void vmsger_deinit(struct vmsger*);
void vmsger_reg_pack_cb  (struct vmsger*, vmsger_pack_t,   void*);
void vmsger_reg_unpack_cb(struct vmsger*, vmsger_unpack_t, void*);

#endif
