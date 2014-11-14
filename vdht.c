#include "vglobal.h"
#include "vdht.h"

static
struct vdhtId_desc dhtId_desc[] = {
    {VDHT_PING,                 "ping"                  },
    {VDHT_PING_R,               "ping_rsp"              },
    {VDHT_FIND_NODE,            "find_node"             },
    {VDHT_FIND_NODE_R,          "find_node_rsp"         },
    {VDHT_GET_PEERS,            "get_peers"             },
    {VDHT_GET_PEERS_R,          "get_peers_rsp"         },
    {VDHT_POST_HASH,            "post_hash"             },
    {VDHT_POST_HASH_R,          "post_hash_rsp"         },
    {VDHT_FIND_CLOSEST_NODES,   "find_closest_nodes"    },
    {VDHT_FIND_CLOSEST_NODES_R, "find_closest_nodes_rsp"},
    {VDHT_GET_PLUGIN,           "get_plugin"            },
    {VDHT_GET_PLUGIN_R,         "get_plugin_rsp"        },
    {VDHT_UNKNOWN, NULL}
};

static
struct be_node* _aux_be_create_info(vnodeInfo* info)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    vassert(info);

    dict = be_create_dict();
    retE_p((!dict));

    node = be_create_vnodeId(&info->id);
    be_add_keypair(dict, "id", node);

    node = be_create_addr(&info->addr);
    be_add_keypair(dict, "m", node);

    node = be_create_ver(&info->ver);
    be_add_keypair(dict, "v", node);

    node = be_create_int(info->flags);
    be_add_keypair(dict, "f", node);

    return dict;
}

/*
 * @token:
 * @srcId: Id of node sending query.
 * @buf:
 * @len:
 *
 * ping Query = {"t":"80407320171565445232",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"78876038483004838102"}}
 * bencoded = d1:t20:804073201715654452321:y1:q1:q4:ping1:ad2:id20:7887603848300
 * 4838102ee
 */
static
int _vdht_enc_ping(vtoken* token, vnodeId* srcId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* id   = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("ping");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    id   = be_create_vnodeId(srcId);
    be_add_keypair(node, "id", id);
    be_add_keypair(dict, "a", node);

    ret  = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId: Id of node replying query.
 * @result: queried result
 * @buf:
 * @len:
 *
 * response = {"t":"875675086641542182221",
 *             "y":"r",
 *             "r":{"node" :{"id": "74281510116552046580",
 *                            "m": "192.168.4.125:12300",
 *                            "v": "9:0.0.0.1.0",
 *                            "f": "1023"
 *                          }
 *                 }
 *            }
 * bencoded = d1:t20:875675086641542182221:y1:r1:rd4:noded2:id20:742815101165520
 * 465801:m19:192.168.4.125:123001:v9:0.0.0.1.01:fi1023eeee
 */
static
int _vdht_enc_ping_rsp(vtoken* token, vnodeInfo* result,void* buf, int   sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    int ret = 0;

    vassert(token);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = _aux_be_create_info(result);
    be_add_keypair(rslt, "node", node);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId: Id of node sending query.
 * @target: Id of queried node.
 * @buf:
 * @len:
 *
 * find_node Query = {"t":"aa",
 *                    "y":"q",
 *                    "q":"find_node",
 *                    "a": {"id":"abcdefghij0123456789",
 *                          "target":"mnopqrstuvwxyz123456"
 *                         }
 *                   }
 * bencoded = d1:ad2:id20:abcdefghij01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe
 */
static
int _vdht_enc_find_node(vtoken* token, vnodeId* srcId, vnodeId* targetId, void* buf,int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("find_node");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    tmp  = be_create_vnodeId(srcId);
    be_add_keypair(node, "id", tmp);
    tmp  = be_create_vnodeId(targetId);
    be_add_keypair(node, "target", tmp);
    be_add_keypair(dict, "a", node);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId: Id of node replying query
 * @result: queried result.
 * @buf:
 * @sz:
 *
 *  response = {"t":"aa",
 *             "y":"r",
 *             "r":{"id":"abcdefghij0123456789",
 *                  "node" :{"id": "abcdefghij0123456789",
 *                            "m": "0120342301031234",
 *                            "v": "20013243413143414",
 *                            "f": "00000001"
 *                          }
 *                 }
 *            }
 * bencoded = d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe
 */
static
int _vdht_enc_find_node_rsp(vtoken* token, vnodeId* srcId, vnodeInfo* result, void* buf,int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_vnodeId(srcId);
    be_add_keypair(rslt, "id", node);

    node = _aux_be_create_info(result);
    be_add_keypair(rslt, "node", node);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token :
 * @src : id of source node that send ping query.
 * @hash:
 * @buf:
 * @len:
 */
static
int _vdht_enc_get_peers(vtoken* token,vnodeId* srcId, vnodeHash* hash, void* buf, int sz)
{
    vassert(token);
    vassert(srcId);
    vassert(hash);
    vassert(buf);
    vassert(sz> 0);
    //todo;
    return 0;
}

/*
 * @token :
 * @srcId :
 * @result:
 * @buf:
 * @sz:
 */
static
int _vdht_enc_get_peers_rsp(
        vtoken* token,
        vnodeId* srcId,
        struct varray* result,
        void* buf,
        int sz)
{
    vassert(token);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);
    //todo;
    return 0;
}

/*
 * @token:
 * @srcId: Id of node sending query.
 * @target: Id of queried node.
 * @buf:
 * @len:
 *
 * find_node Query = {"t":"aa",
 *                    "y":"q",
 *                    "q":"find_closest_nodes",
 *                    "a": {"id":"abcdefghij0123456789",
 *                          "target":"mnopqrstuvwxyz123456"
 *                         }
 *                   }
 * bencoded = d1:ad2:id20:abcdefghij01234567896:target20:mnopqrstuvwxyz123456e1:q9:find_node1:t2:aa1:y1:qe
 */
static
int _vdht_enc_find_closest_nodes(vtoken* token, vnodeId* srcId, vnodeId* targetId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("find_closest_nodes");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    tmp  = be_create_vnodeId(srcId);
    be_add_keypair(node, "id", tmp);
    tmp  = be_create_vnodeId(targetId);
    be_add_keypair(node, "target", tmp);
    be_add_keypair(dict, "a", node);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId: Id of node replying query
 * @result: queried result.
 * @buf:
 * @sz:
 *
 *  response = {"t":"aa",
 *              "y":"r",
 *              "r":{"id":"abcdefghij0123456789",
 *                  "nodes" :["id": "abcdefghij0123456789",
 *                            "m": "0120342301031234",
 *                            "v": "20013243413143414",
 *                            "f": "00000001"
 *                          ],
 *                          ["id": "jddafiejklj0123456789",
 *                            "m": "0120342301031234",
 *                            "v": "21013243413143414",
 *                            "f": "00000101"
 *                          ],...
 *                 }
 *            }
 * bencoded = d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe
 */
static
int _vdht_enc_find_closest_nodes_rsp(
        vtoken* token,
        vnodeId* srcId,
        struct varray* closest,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    struct be_node* list = NULL;
    int ret = 0;
    int i   = 0;

    vassert(token);
    vassert(srcId);
    vassert(closest);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_vnodeId(srcId);
    be_add_keypair(rslt, "id", node);
    list = be_create_list();
    for (; i < varray_size(closest); i++) {
        vnodeInfo* info = NULL;

        info = (vnodeInfo*)varray_get(closest, i);
        node = _aux_be_create_info(info);
        be_add_list(list, node);
    }
    be_add_keypair(rslt, "nodes", list);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}


 /* 1. request special plug info
 *  Query = { "t": "80407320171565445232",
 *            "y": "q",
 *            "q": "get_plugin"
 *            "a": {"id: "1"}
 *           }
 */
static
int _vdht_enc_get_plugin(vtoken* token, int plgnId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* id   = NULL;
    int ret = 0;

    vassert(token);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node= be_create_str("get_plugin");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    id   = be_create_int(plgnId);
    be_add_keypair(node, "id", id);
    be_add_keypair(dict, "a", node);;

    ret  = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/* 1. request special plug info
 *  Query = { "t": "80407320171565445232",
 *            "y": "r",
 *            "r": {"plugin" {"id": "1",
 *                            "m" : "10.0.0.16:13400"
 *                           }
 *                 }
 *           }
 */
static
int _vdht_enc_get_plugin_rsp(vtoken* token, int plgnId, struct sockaddr_in* addr, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    struct be_node* plgn = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(addr);
    vassert(plgnId >= 0);
    vassert(plgnId < PLUGIN_BUTT);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);
    node = be_create_str("r");
    be_add_keypair(dict, "y", node);

    plgn = be_create_dict();
    node = be_create_int(plgnId);
    be_add_keypair(plgn, "id", node);
    node = be_create_addr(addr);
    be_add_keypair(plgn, "m", node);

    rslt = be_create_dict();
    be_add_keypair(rslt, "plugin", plgn);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

struct vdht_enc_ops dht_enc_ops = {
    .ping                   = _vdht_enc_ping,
    .ping_rsp               = _vdht_enc_ping_rsp,
    .find_node              = _vdht_enc_find_node,
    .find_node_rsp          = _vdht_enc_find_node_rsp,
    .get_peers              = _vdht_enc_get_peers,
    .get_peers_rsp          = _vdht_enc_get_peers_rsp,
    .find_closest_nodes     = _vdht_enc_find_closest_nodes,
    .find_closest_nodes_rsp = _vdht_enc_find_closest_nodes_rsp,
    .get_plugin             = _vdht_enc_get_plugin,
    .get_plugin_rsp         = _vdht_enc_get_plugin_rsp
};

static
int _aux_unpack_token(struct be_node* dict, vtoken* token)
{
    struct be_node* node = NULL;
    int ret = 0;
    vassert(dict);
    vassert(token);

    ret = be_node_by_key(dict, "t", &node);
    retE((ret < 0));
    retE((BE_STR != node->type));

    ret  = be_unpack_token(node, token);
    retE((ret < 0));
    return 0;
}


static
int _aux_unpack_vnodeId(struct be_node* dict, char* key1, char* key2, vnodeId* id)
{
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(key1);
    vassert(key2);
    vassert(id);

    ret = be_node_by_2keys(dict, key1, key2, &node);
    retE((ret < 0));
    retE((BE_STR != node->type));

    ret = be_unpack_nodeId(node, id);
    retE((ret < 0));
    return 0;
}

static
int _aux_unpack_vnodeInfo(struct be_node* dict, vnodeInfo* info)
{
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(info);
    retE((BE_DICT != dict->type));

    ret = be_node_by_key(dict, "id", &node);
    retE((ret < 0));
    ret = be_unpack_nodeId(node, &info->id);
    retE((ret < 0));
    ret = be_node_by_key(dict, "m", &node);
    retE((ret < 0));
    ret = be_unpack_addr(node, &info->addr);
    retE((ret < 0));
    ret = be_node_by_key(dict, "v", &node);
    retE((ret < 0));
    ret = be_unpack_version(node, &info->ver);
    retE((ret < 0));
    ret = be_node_by_key(dict, "f", &node);
    retE((ret < 0));
    ret = be_unpack_int(node, (int*)&info->flags);
    retE((ret < 0));
    return 0;
}

static
int _aux_unpack_dhtId(struct be_node* dict)
{
    struct vdhtId_desc* desc = dhtId_desc;
    struct be_node* node = NULL;
    int dhtId = 0;
    int ret = 0;
    int i = 0;

    vassert(dict);
    retE((BE_DICT != dict->type));

    ret = be_node_by_key(dict, "y", &node);
    retE((ret < 0));
    retE((BE_STR != node->type));

    if (!strcmp(node->val.s, "q")) {
        ret = be_node_by_key(dict, "q", &node);
        retE((ret < 0));
        retE((BE_STR != node->type));

        dhtId = VDHT_UNKNOWN;
        for (; desc->desc; ) {
            if (!strcmp(node->val.s, desc->desc)) {
                dhtId = desc->dhtId;
                break;
            }
            desc++;
        }
        return dhtId;
    }
    if (!strcmp(node->val.s, "r")) {
        ret = be_node_by_2keys(dict, "r", "nodes", &node);
        if (!ret) {
            return VDHT_FIND_CLOSEST_NODES_R;
        }
        ret = be_node_by_2keys(dict, "r", "hash", &node);
        if (!ret) {
            return VDHT_GET_PEERS_R;
        }
        ret = be_node_by_2keys(dict, "r", "plugin", &node);
        if (!ret) {
            return VDHT_GET_PLUGIN_R;
        }
        ret = be_node_by_2keys(dict, "r", "node", &node);
        if (ret < 0) {
            return VDHT_UNKNOWN;
        }
        ret = be_node_by_2keys(dict, "r", "id", &node);
        if (ret < 0) {
            return VDHT_PING_R;
        }
        return VDHT_FIND_NODE_R;
    }

    return VDHT_UNKNOWN;
}

/*
 * @buf:
 * @len:
 * @token:
 * @srcId
 *
 * ping Query = {"t":"80407320171565445232",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"78876038483004838102"}}
 * bencoded = d1:t20:804073201715654452321:y1:q1:q4:ping1:ad2:id20:7887603848300
 * 4838102ee
 */
static
int _vdht_dec_ping(void* ctxt, vtoken* token, vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret  = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);

    ret = _aux_unpack_token(dict, token);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));

    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @result:
 * response = {"t":"875675086641542182221",
 *             "y":"r",
 *             "r":{"node" :{"id": "74281510116552046580",
 *                            "m": "19:192.168.4.125:12300",
 *                            "v": "9:0.0.0.1.0",
 *                            "f": "1023"
 *                          }
 *                 }
 *            }
 * bencoded = d1:t20:875675086641542182221:y1:r1:rd4:noded2:id20:742815101165520
 * 465801:m19:192.168.4.125:123001:v9:0.0.0.1.01:fi1023eeee
 */
static
int _vdht_dec_ping_rsp(void* ctxt, vtoken* token, vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(result);

    ret = _aux_unpack_token(dict, token);
    retE((ret < 0));

    ret = be_node_by_2keys(dict, "r", "node", &node);
    retE((ret < 0));
    ret = _aux_unpack_vnodeInfo(node, result);
    retE((ret < 0));

    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @srcId:
 * @targetId:
 */
static
int _vdht_dec_find_node(void* ctxt, vtoken* token, vnodeId* srcId, vnodeId* targetId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    ret = _aux_unpack_token(dict, token);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "target", targetId);
    retE((ret < 0));

    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @srcId:
 * @result:
 *
 * response = {"t":"aa",
 *             "y":"r",
 *             "r":{"id":"abcdefghij0123456789",
 *                  "node" :{"id": "abcdefghij0123456789",
 *                            "m": "0120342301031234",
 *                            "v": "20013243413143414",
 *                            "f": "00000001"
 *                          }
 *                 }
 *            }
 * bencoded = d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe
 */
static
int _vdht_dec_find_node_rsp(void* ctxt, vtoken* token, vnodeId* srcId, vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(result);

    ret = _aux_unpack_token(dict, token);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "r", "id", srcId);
    retE((ret < 0));

    ret = be_node_by_2keys(dict, "r", "node", &node);
    retE((ret < 0));
    ret = _aux_unpack_vnodeInfo(node, result);
    retE((ret < 0));

    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @srcId:
 * @hash:
 */
static
int _vdht_dec_get_peers(void* ctxt, vtoken* token, vnodeId* srcId, vnodeHash* hash)
{
    vassert(token > 0);
    vassert(srcId);
    vassert(hash);

    //todo;
    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @srcId:
 * @result:
 */
static
int _vdht_dec_get_peers_rsp(void* ctxt, vtoken* token, vnodeId* srcId, struct varray* result)
{
    vassert(token);
    vassert(srcId);
    vassert(result);

    //todo;
    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @srcId:
 * @closest:
 */
static
int _vdht_dec_find_closest_nodes(void* ctxt, vtoken* token, vnodeId* srcId, vnodeId* targetId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    ret = _aux_unpack_token(dict, token);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "target", targetId);
    retE((ret < 0));

    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @srcId:
 * @closest:
 */
static
int _vdht_dec_find_closest_nodes_rsp(void* ctxt, vtoken* token, vnodeId* srcId, struct varray* closest)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* list = NULL;
    struct be_node* node = NULL;
    int ret = 0;
    int i = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(closest);

    ret = _aux_unpack_token(dict, token);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "r", "id", srcId);
    retE((ret < 0));

    ret = be_node_by_2keys(dict, "r", "node", &list);
    retE((ret < 0));
    retE((BE_LIST != list->type));

    for (; node->val.l[i]; i++) {
        vnodeInfo* info = NULL;

        node = list->val.l[i];
        retE((BE_DICT != node->type));
        info = vnodeInfo_alloc();
        retE((!info));

        ret = _aux_unpack_vnodeInfo(node, info);
        ret1E((ret < 0), vnodeInfo_free(info));
        varray_add_tail(closest, info);
    }

    return 0;
}

/*
 *  1. @get_plugin message format.
 *
 *  Query = { "t": "80407320171565445232",
 *            "y": "q",
 *            "q": "get_plugin"
 *            "a": {"id: "1"}
 *           }
 *
 * @ctxt : dht decode context
 * @token: token to dht message
 * @plugId: plugin ID
 *
 */
static
int _vdht_dec_get_plugin(void* ctxt, vtoken* token, int* plgnId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(plgnId);

    ret = be_node_by_key(dict, "t", &node);
    retE((ret < 0));
    retE((BE_STR != node->type));

    ret = be_unpack_token(node, token);
    retE((ret < 0));

    ret = be_node_by_2keys(dict, "a", "id", &node);
    retE((ret < 0));
    retE((BE_INT != node->type));

    *plgnId = node->val.i;
    return 0;
}

/* @get_plugin_rsp message format
 *
 * 1. success:
 *  Response = { "t": "80407320171565445232",
 *               "y": "r",
 *               "r": {"plugin": {"id": "1",
 *                                "m" : "10.0.0.16:13400"
 *                               }
 *                    }
 *              }
 * 2. failed:
 *  Response = { "t": "80407320171565445232",
 *               "y": "r",
 *               "r": {"plugin": {}
 *                    }
 *              }
 *
 * @ctxt  : dht decoder context
 * @token : token to dht message.
 * @plgnId: plugin ID
 * @addr  : addr to plugin server
 *
 */
static
int _vdht_dec_get_plugin_rsp(void* ctxt, vtoken* token, int* plgnId, struct sockaddr_in* addr)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* plgn = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(plgnId);
    vassert(addr);

    ret = be_node_by_key(dict, "t", &node);
    retE((ret < 0));
    retE((BE_STR != node->type));

    ret = be_node_by_2keys(dict, "r", "plugin", &plgn);
    retE((ret < 0));
    retE((BE_DICT != plgn->type));

    ret = be_node_by_key(plgn, "id", &node);
    retE((ret < 0));
    retE((BE_INT != plgn->type));
    *plgnId = node->val.i;

    ret = be_node_by_key(plgn, "m", &node);
    retE((ret < 0));
    retE((BE_STR != node->type));
    ret = be_unpack_addr(node, addr);
    retE((ret < 0));

    return 0;
}

static
int _vdht_dec(void* buf, int len, void** ctxt)
{
    struct be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(len);

    vlogI(printf("[dht msg]->%s", (char*)buf));
    dict = be_decode(buf, len);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = _aux_unpack_dhtId(dict);
    retE((ret < 0));

    *ctxt = (void*)dict;
    return ret;
}

static
int _vdht_dec_done(void* ctxt)
{
    vassert(ctxt);
    be_free((struct be_node*)ctxt);
    return 0;
}

struct vdht_dec_ops dht_dec_ops = {
    .dec                    = _vdht_dec,
    .dec_done               = _vdht_dec_done,

    .ping                   = _vdht_dec_ping,
    .ping_rsp               = _vdht_dec_ping_rsp,
    .find_node              = _vdht_dec_find_node,
    .find_node_rsp          = _vdht_dec_find_node_rsp,
    .get_peers              = _vdht_dec_get_peers,
    .get_peers_rsp          = _vdht_dec_get_peers_rsp,
    .find_closest_nodes     = _vdht_dec_find_closest_nodes,
    .find_closest_nodes_rsp = _vdht_dec_find_closest_nodes_rsp,
    .get_plugin             = _vdht_dec_get_plugin,
    .get_plugin_rsp         = _vdht_dec_get_plugin_rsp
};

void* vdht_buf_alloc(void)
{
    void* buf = NULL;

    buf = malloc(8*BUF_SZ);
    vlog((!buf), elog_malloc);
    retE_p((!buf));
    memset(buf, 0, 8*BUF_SZ);

    return buf + 8;
}

int vdht_buf_len(void)
{
    return (int)(8*BUF_SZ - 8);
}

void vdht_buf_free(void* buf)
{
    vassert(buf);

    free(buf - 8);
    return ;
}

