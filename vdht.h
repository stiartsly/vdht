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
    VDHT_PROBE,
    VDHT_PROBE_R,
    VDHT_POST_SERVICE,
    VDHT_FIND_SERVICE,
    VDHT_FIND_SERVICE_R,
    VDHT_UNKNOWN
};

/*
 * method set for dht msg encoder
 */
struct vdht_enc_ops {
    int (*ping)(
            vnonce* nonce,  // transaction Id.
            vnodeId* srcId, // node Id querying
            void* buf,
            int   sz);

    int (*ping_rsp)(
            vnonce* nonce,  // trans Id.
            vnodeId* srcId,
            vnodeInfo* result, // query result. nomally info of current dht node
            void* buf,
            int   sz);

    int (*find_node)(
            vnonce* nonce,
            vnodeId* srcId, // node Id querying.
            vnodeId* targetId,  // Id of queried target
            void* buf,
            int   sz);

    int (*find_node_rsp)(
            vnonce* nonce,
            vnodeId* srcId, // node Id replying query.
            vnodeInfo* result, // query result, normally info the target dht node.
            void* buf,
            int   sz);

    int (*find_closest_nodes)(
            vnonce* nonce,
            vnodeId* srcId,
            vnodeId* targetId,
            void* buf,
            int   sz);
    int (*find_closest_nodes_rsp)(
            vnonce* nonce,
            vnodeId* srcId,
            struct varray* result,
            void* buf,
            int   sz);

    int (*reflex)(
            vnonce* nonce,
            vnodeId* srcId,
            void* buf,
            int   sz);

    int (*reflex_rsp)(
            vnonce* nonce,
            vnodeId* srcId,
            struct vsockaddr_in* reflexive_addr,
            void* buf,
            int   sz);

    int (*probe)(
            vnonce* nonce,
            vnodeId* srcId,
            vnodeId* destId,
            void* buf,
            int   sz);

    int (*probe_rsp)(
            vnonce* nonce,
            vnodeId* srcId,
            void* buf,
            int   sz);

    int (*post_service)(
            vnonce* nonce,
            vnodeId* srcId,
            vsrvcInfo* srvc,
            void* buf,
            int   sz);

    int (*find_service)(
            vnonce* nonce,
            vnodeId* srcId,
            vsrvcHash* hash,
            void* buf,
            int   sz);

    int (*find_service_rsp)(
            vnonce* nonce,
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
            void** ctxt);

    int (*dec_done)(
            void* ctxt);

    int (*ping)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId);

    int (*ping_rsp)(
            void* ctxt,
            vnonce* nonce,
            vnodeInfo* result);

    int (*find_node)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vnodeId* targetId); // queried vnode Id.

    int (*find_node_rsp)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vnodeInfo* result);

    int (*find_closest_nodes)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vnodeId* targetId);

    int (*find_closest_nodes_rsp)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            struct varray* result);

    int (*reflex)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId);

    int (*reflex_rsp)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            struct vsockaddr_in* reflexive_addr);

    int (*probe)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vnodeId* destId);

    int (*probe_rsp)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId);

    int (*post_service) (
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vsrvcInfo* srvci);

    int (*find_service)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vsrvcHash* target_hash);

    int (*find_service_rsp)(
            void* ctxt,
            vnonce* nonce,
            vnodeId* srcId,
            vsrvcInfo* srvci);
};

char* vdht_get_desc(int);
int   vdht_get_dhtId_by_desc(const char*);
void* vdht_buf_alloc(void);
int   vdht_buf_len(void);
void  vdht_buf_free(void*);

#endif
