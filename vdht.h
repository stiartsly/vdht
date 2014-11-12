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
    VDHT_GET_PEERS,
    VDHT_GET_PEERS_R,
    VDHT_POST_HASH,
    VDHT_POST_HASH_R,
    VDHT_FIND_CLOSEST_NODES,
    VDHT_FIND_CLOSEST_NODES_R,
    VDHT_UNKNOWN
};

struct vdhtId_desc {
    int   dhtId;
    char* desc;
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

    int (*get_peers)(
            vtoken* token,
            vnodeId* srcId,
            vnodeHash* hash,
            void* buf,
            int   sz);

    int (*get_peers_rsp)(
            vtoken* token,
            vnodeId* srcId,
            struct varray* result,
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
};

/*
 * method set for dht msg decoder
 */

struct vdht_dec_ops {
    int (*dec) (
            void* buf,
            int sz,
            void** ctxt
        );

    int (*dec_done)(
            void* ctxt
        );

    int (*ping)(
            void* ctxt,
            vtoken* token, // transaction Id for the query.
            vnodeId* srcId // where query is from.
        );

    int (*ping_rsp)(
            void* ctxt,
            vtoken* token,
            vnodeInfo* result
        );

    int (*find_node)(
            void* ctxt,
            vtoken* token,
            vnodeId* srcId,   // quering vnode id,
            vnodeId* targetId // queried vnode Id.
        );

    int (*find_node_rsp)(
            void* ctxt,
            vtoken* token,
            vnodeId* srcId,
            vnodeInfo* result
        );

    int (*get_peers)(
            void* ctxt,
            vtoken* token,
            vnodeId* srcId,
            vnodeHash* hash
        );

    int (*get_peers_rsp)(
            void* ctxt,
            vtoken* token,
            vnodeId* srcId,
            struct varray* result
        );

    int (*find_closest_nodes)(
            void* ctxt,
            vtoken* token,
            vnodeId* srcId,
            vnodeId* targetId
        );

    int (*find_closest_nodes_rsp)(
            void* ctxt,
            vtoken* token,
            vnodeId* srcId,
            struct varray* result
        );
};

#endif
