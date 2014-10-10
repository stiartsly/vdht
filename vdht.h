#ifndef __VDHT_H__
#define __VDHT_H__

#include "varray.h"
#include "vnodeId.h"

#define DHT_MAGIC ((uint32_t)0x58681506)
#define IS_DHT_MSG(magic) (magic == DHT_MAGIC)

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
            vnodeId* srcId,
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
    int (*ping)(
            void* buf, // msg buf address,
            int   sz,  // buf size
            vtoken* token, // transaction Id for the query.
            vnodeId* srcId // where query is from.
        );

    int (*ping_rsp)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,
            vnodeInfo* result
        );

    int (*find_node)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,   // quering vnode id,
            vnodeId* targetId // queried vnode Id.
        );

    int (*find_node_rsp)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,
            vnodeInfo* result
        );

    int (*get_peers)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,
            vnodeHash* hash
        );

    int (*get_peers_rsp)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,
            struct varray* result
        );

    int (*find_closest_nodes)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,
            vnodeId* targetId
        );

    int (*find_closest_nodes_rsp)(
            void* buf,
            int   sz,
            vtoken* token,
            vnodeId* srcId,
            struct varray* result
        );
};

#endif
