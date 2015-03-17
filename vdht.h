#ifndef __VDHT_H__
#define __VDHT_H__

#include "varray.h"
#include "vnodeId.h"

#define DHT_MAGIC ((uint32_t)0x58681506)
#define IS_DHT_MSG(magic) (magic == DHT_MAGIC)

enum {
    VDHT_PING,
    VDHT_PING_R,
    VDHT_FIND_NODE,
    VDHT_FIND_NODE_R,
    VDHT_FIND_CLOSEST_NODES,
    VDHT_FIND_CLOSEST_NODES_R,
    VDHT_REFLEX,
    VDHT_REFLEX_R,
    VDHT_POST_SERVICE,
    VDHT_UNKNOWN
};

/*
 * method set for dht msg encoder
 */
struct vdht_enc_ops {
    int (*ping)(
            vtoken* token,   // transaction Id.
            vnodeId* srcId, // node Id querying
            void* buf,
            int   sz);

    int (*ping_rsp)(
            vtoken* token,  // trans Id.
            vnodeInfo* result, // query result. nomally info of current dht node
            void* buf,
            int   sz);

    int (*find_node)(
            vtoken* token,
            vnodeId* srcId, // node Id querying.
            vnodeId* targetId,  // Id of queried target
            void* buf,
            int   sz);

    int (*find_node_rsp)(
            vtoken* token,
            vnodeId* srcId, // node Id replying query.
            vnodeInfo* result, // query result, normally info the target dht node.
            void* buf,
            int   sz);

    int (*find_closest_nodes)(
            vtoken* token,
            vnodeId* srcId,
            vnodeId* targetId,
            void* buf,
            int   sz);
    int (*find_closest_nodes_rsp)(
            vtoken* token,
            vnodeId* srcId,
            struct varray* result,
            void* buf,
            int   sz);

    int (*reflex)(
            vtoken* token,
            vnodeId* srcId,
            void* buf,
            int sz);

    int (*reflex_rsp)(
            vtoken* token,
            vnodeId* srcId,
            struct sockaddr_in* reflexive_addr,
            void* buf,
            int sz);

    int (*post_service)(
            vtoken* token,
            vnodeId* srcId,
            vsrvcInfo* srvc,
            void* buf,
            int   sz);

};

/*
 * method set for dht msg decoder
 */

struct vdht_dec_ops {
    int (*dec_begin) (
            void* buf,
            int sz,
            vtoken* token,
            vnodeId* srcId,
            void** ctxt
        );

    int (*dec_done)(
            void* ctxt
        );

    int (*ping)(
            void* ctxt
        );

    int (*ping_rsp)(
            void* ctxt,
            vnodeInfo* result
        );

    int (*find_node)(
            void* ctxt,
            vnodeId* targetId // queried vnode Id.
        );

    int (*find_node_rsp)(
            void* ctxt,
            vnodeInfo* result
        );

    int (*find_closest_nodes)(
            void* ctxt,
            vnodeId* targetId
        );

    int (*find_closest_nodes_rsp)(
            void* ctxt,
            struct varray* result
        );

    int (*reflex)(
            void* ctxt
        );

    int (*reflex_rsp)(
            void* ctxt,
            struct sockaddr_in* reflexive_addr
        );

    int (*post_service) (
            void* ctxt,
            vsrvcInfo** service
        );
};

char* vdht_get_desc(int);
int   vdht_get_dhtId_by_desc(const char*);
void* vdht_buf_alloc(void);
int   vdht_buf_len(void);
void  vdht_buf_free(void*);

#endif
