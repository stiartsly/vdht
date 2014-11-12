#include "vglobal.h"
#include "vdht.h"

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

struct vdht_enc_ops dht_enc_ops = {
    .ping                   = _vdht_enc_ping,
    .ping_rsp               = _vdht_enc_ping_rsp,
    .find_node              = _vdht_enc_find_node,
    .find_node_rsp          = _vdht_enc_find_node_rsp,
    .get_peers              = _vdht_enc_get_peers,
    .get_peers_rsp          = _vdht_enc_get_peers_rsp,
    .find_closest_nodes     = _vdht_enc_find_closest_nodes,
    .find_closest_nodes_rsp = _vdht_enc_find_closest_nodes_rsp
};

static
struct be_node* _aux_be_get_node(struct be_node* dict, char* key1, char* key2)
{
    struct be_node* node = NULL;
    vassert(dict);
    vassert(key1);
    vassert(key2);

    node = be_get_dict(dict, key1);
    retE_p((!node));
    retE_p((BE_DICT != node->type));

    node = be_get_dict(node, key2);
    retE_p((!node));
    return node;
}

static
int _aux_be_get_id(struct be_node* dict, char* key1, char* key2, vnodeId* id)
{
    struct be_node* node = NULL;
    int len = 0;
    int ret = 0;

    vassert(dict);
    vassert(key1);
    vassert(key2);
    vassert(id);

    node = _aux_be_get_node(dict, key1, key2);
    retE((!node));
    retE((BE_STR != node->type));

    len = get_int32(unoff_addr(node->val.s, sizeof(int32_t)));
    retE((len != strlen(node->val.s)));

    ret = vnodeId_unstrlize(node->val.s, id);
    retE((ret < 0));
    return 0;
}

static
int _aux_be_get_info(struct be_node* dict, vnodeInfo* info)
{
    struct be_node* node = NULL;
    int len = 0;
    int ret = 0;
    vassert(dict);
    vassert(info);

    node = be_get_dict(dict, "id");
    retE((!node));
    retE((BE_STR != node->type));

    len = get_int32(unoff_addr(node->val.s, sizeof(int32_t)));
    retE((len != strlen(node->val.s)));
    ret = vnodeId_unstrlize(node->val.s, &info->id);
    retE((ret < 0));

    node = be_get_dict(dict, "m");
    retE((!node));
    retE((BE_STR != node->type));
    len = get_int32(unoff_addr(node->val.s, sizeof(int32_t)));
    retE((len != strlen(node->val.s)));
    {
        char* s = strchr(node->val.s, ':');
        char ip[64];
        int port = 0;
        retE((!s));
        memset(ip, 0, 64);
        strncpy(ip, node->val.s, s - node->val.s);
        s += 1;
        errno = 0;
        port = strtol(s, NULL, 10);
        retE((errno));
        ret = vsockaddr_convert(ip, port, &info->addr);
        retE((ret < 0));
    }

    node = be_get_dict(dict, "v"); // version.
    retE((!node));
    retE((BE_STR != node->type));
    len = get_int32(unoff_addr(node->val.s, sizeof(int32_t)));
    retE((len != strlen(node->val.s)));
    ret = vnodeVer_unstrlize(node->val.s, &info->ver);
    retE((ret < 0));

    node = be_get_dict(dict, "f"); // flags;
    retE((!node));
    retE((BE_INT != node->type));
    info->flags = (uint32_t)node->val.i;
    return 0;
}

static
int _aux_be_get_mtype(struct be_node* dict, int* mtype)
{
    struct be_node* node = NULL;

    vassert(dict);
    vassert(mtype);

    node = be_get_dict(dict, "y");
    retE((!node));
    retE((BE_STR != node->type));

    if (!strcmp(node->val.s, "q")) {
        node = be_get_dict(dict, "q");
        retE((!node));
        retE((BE_STR != node->type));

        if (!strcmp(node->val.s, "ping")) {
            *mtype = VDHT_PING;
        } else if (!strcmp(node->val.s, "find_node")) {
            *mtype = VDHT_FIND_NODE;
        } else if (!strcmp(node->val.s, "get_peers")) {
            *mtype = VDHT_GET_PEERS;
        } else if (!strcmp(node->val.s, "find_closest_nodes")) {
            *mtype = VDHT_FIND_CLOSEST_NODES;
        } else {
            *mtype = VDHT_UNKNOWN;
            retE((1));
        }
        return 0;

    } else if (!strcmp(node->val.s, "r")) {
        struct be_node* tmp = NULL;
        node = be_get_dict(dict, "r");
        retE((!node));
        retE((BE_DICT != node->type));
        tmp = be_get_dict(node, "node");
        if (tmp) {
            tmp = be_get_dict(node, "id");
            if (!tmp) {
                *mtype = VDHT_PING_R;
            }else {
                *mtype = VDHT_FIND_NODE_R;
            }
            return 0;
        }
        tmp = be_get_dict(node, "hash");
        if (tmp) {
            *mtype = VDHT_GET_PEERS_R;
            return 0;
        }
        tmp = be_get_dict(node, "nodes");
        if (tmp) {
            *mtype = VDHT_FIND_CLOSEST_NODES_R;
            return 0;
        }
        *mtype = VDHT_UNKNOWN;
        retE((1));

    } else {
        *mtype = VDHT_UNKNOWN;
        retE((1));
    }
    return 0;
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

    ret = be_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "a", "id", srcId);
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

    ret = be_get_token(dict, token);
    retE((ret < 0));

    node = _aux_be_get_node(dict, "r", "node");
    retE((!node));
    ret  = _aux_be_get_info(node, result);
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

    ret = be_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "a", "target", targetId);
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

    ret = be_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "r", "id", srcId);
    retE((ret < 0));

    node = _aux_be_get_node(dict, "r", "node");
    retE((!node));
    ret  = _aux_be_get_info(node, result);
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

    ret = be_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "a", "target", targetId);
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

    ret = be_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_be_get_id(dict, "r", "id", srcId);
    retE((ret < 0));

    list = _aux_be_get_node(dict, "r", "nodes");
    retE((ret < 0));
    retE((BE_LIST != list->type));

    for (; node->val.l[i]; i++) {
        vnodeInfo* info = NULL;

        node = list->val.l[i];
        retE((BE_DICT != node->type));
        info = vnodeInfo_alloc();
        retE((!info));

        ret = _aux_be_get_info(node, info);
        ret1E((ret < 0), vnodeInfo_free(info));
        varray_add_tail(closest, info);
    }

    return 0;
}

static
int _vdht_dec(void* buf, int len, void** ctxt)
{
    struct be_node* dict = NULL;
    int mtype = 0;
    int ret = 0;

    vassert(buf);
    vassert(len);

    vlogI(printf("[dht msg]->%s", (char*)buf));
    dict = be_decode(buf, len);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = _aux_be_get_mtype(dict, &mtype);
    retE((ret < 0));

    *ctxt = (void*)dict;
    return mtype;
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
    .find_closest_nodes_rsp = _vdht_dec_find_closest_nodes_rsp
};

