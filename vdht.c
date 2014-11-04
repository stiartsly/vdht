#include "vglobal.h"
#include "vdht.h"

enum {
    BE_STR,
    BE_INT,
    BE_LIST,
    BE_DICT,
    BE_BUT
};

struct be_node;
struct be_dict {
    char* key;
    struct be_node* val;
};

struct be_node {
    int type;
    int capc;
    union {
        char* s;
        long i;
        struct be_node **l;
        struct be_dict *d;
    }val;
};

static MEM_AUX_INIT(be_cache, sizeof(struct be_node), 16);
static
struct be_node* be_alloc(int type)
{
    struct be_node* node = NULL;
    vassert(type >= BE_STR);
    vassert(type <  BE_BUT);

    node = (struct be_node*)vmem_aux_alloc(&be_cache);
    vlog((!node), elog_vmem_aux_alloc);
    retE_p((!node));

    memset(node, 0, sizeof(*node));
    node->type = type;
    node->capc = 0;
    return node;
}

static
void be_free(struct be_node* node)
{
    vassert(node);
    switch(node->type) {
    case BE_STR:
        free(unoff_addr(node->val.s, sizeof(long)));
        break;
    case BE_INT:
        break;
    case BE_LIST: {
        int i = 0;
        for (; i < node->capc; i++) {
            be_free(node->val.l[i]);
        }
        free(node->val.l);
        break;
    }
    case BE_DICT: {
        int i = 0;
        for (; i < node->capc; i++) {
            free(unoff_addr(node->val.d[i].key, sizeof(long)));
            be_free(node->val.d[i].val);
        }
        free(node->val.d);
        break;
    }
    default:
        vassert(0);
    }
    vmem_aux_free(&be_cache, node);
}


static
long _be_decode_int(char** data, int* data_len)
{
    char *endp = NULL;
    long ret = 0;

    ret   = strtol(*data, &endp, 10);
    *data = endp;
    *data_len -= (endp - *data);
    return ret;
}

static
char* _be_decode_str(char** data, int* data_len)
{
    long len = 0;
    char* s = NULL;

    len = _be_decode_int(data, data_len);
    retE_p((len < 0));
    retE_p((len > *data_len - 1));

    if (**data == ':') {
        char* _s = NULL;

        _s = s = (char*)malloc(sizeof(len) + len + 1);
        vlog((!s), elog_malloc);
        retE_p((!s));

        set_int32(s, len);
        s = offset_addr(_s, sizeof(long));
        memcpy(s, *data + 1, len);
        s[len] = '\0';
        *data += len + 1;
        *data_len -= len -1;
    }
    return s;
}

static
struct be_node *_be_decode(char **data, int *data_len)
{
    struct be_node* node = NULL;

    vassert(*data);
    vassert(data_len);
    vassert(*data_len > 0);

    switch (**data) {
    case 'l': { //list
        int i = 0;
        node = be_alloc(BE_LIST);
        vlog((!node), elog_be_alloc);
        retE_p((!node));

        --(*data_len);
        ++(*data);
        while (**data != 'e') {
            struct be_node** l = NULL;
            l = (struct be_node**)realloc(node->val.l, (i+2)*sizeof(void*));
            vlog((!l), elog_realloc);
            ret1E_p((!l), be_free(node));
            node->val.l = l;

            node->val.l[i] = _be_decode(data, data_len);
            ret1E_p((!node->val.l[i]), be_free(node));
            ++i;
        }
        --(*data_len);
        ++(*data);

        /* empty list case. */
        if (i == 0){
            node->val.l = (struct be_node**)realloc(node->val.l, sizeof(void*));
            vlog((!node->val.d), elog_realloc);
            ret1E_p((!node->val.l), be_free(node));
        }
        node->val.l[i] = NULL;
        break;
    }
    case 'd': { /* dictionaries */
        int i = 0;
        node = be_alloc(BE_DICT);
        vlog((!node), elog_be_alloc);
        retE_p((!node));

        --(*data_len);
        ++(*data);
        while(**data != 'e') {
            struct be_dict* d = NULL;
            d = (struct be_dict*)realloc(node->val.d, (i+2)*sizeof(struct be_dict));
            vlog((!d), elog_realloc);
            ret1E_p((!d), be_free(node));
            node->val.d = d;

            node->val.d[i].key = _be_decode_str(data, data_len);
            ret1E_p((!node->val.d[i].key), be_free(node));
            node->val.d[i].val = _be_decode(data, data_len);
            ret1E_p((!node->val.d[i].val), be_free(node));
            ++i;
        }
        --(*data_len);
        ++(*data);

        if (i == 0) {
            node->val.d = (struct be_dict*)realloc(node->val.d, sizeof(struct be_dict));
            vlog((!node->val.d), elog_realloc);
            ret1E_p((!node->val.d), be_free(node));
        }
        node->val.d[i].val = NULL;
        break;
    }
    case 'i': { /* integers */
        node = be_alloc(BE_INT);
        vlog((!node), elog_be_alloc);
        retE_p((!node));

        --(*data_len);
        ++(*data);
        node->val.i = _be_decode_int(data, data_len);
        retE_p((**data != 'e'));
        --(*data_len);
        ++(*data);
        break;
    }
    case '0'...'9': { /* byte strings */
        node = be_alloc(BE_INT);
        vlog((!node), elog_be_alloc);
        retE_p((!node));

        node->val.s = _be_decode_str(data, data_len);
        ret1E_p((!node->val.s), be_free(node));
        break;
    }
    default: /* invalid*/
        break;
    }
    return node;
}

static
struct be_node *be_decode(char *data, int len)
{
    return _be_decode(&data, &len);
}

static
struct be_node *be_decode_str(char *data)
{
    return be_decode(data, strlen(data));
}

/* hackish way to create nodes! */
static
struct be_node *be_create_dict()
{
    return be_decode_str("de");
}

static
struct be_node *be_create_list()
{
    return be_decode_str("le");
}

static
struct be_node* be_create_str(void* str, int len)
{
    struct be_node* node = NULL;
    char* s = NULL;
    vassert(str);
    vassert(len > 0);

    node = be_alloc(BE_STR);
    vlog((!node), elog_be_alloc);
    retE_p((!node));

    s = (char*)malloc(sizeof(long) + len + 1);
    vlog((!s), elog_malloc);
    ret1E_p((!s), be_free(node));

    ((long*)s)[0] = len;
    s = offset_addr(s, sizeof(long));
    memcpy(s, str, len);
    s[len]='\0';

    node->val.s = (char*)s;
    return node;
}

static
struct be_node *be_create_int(int num)
{
    struct be_node* node = NULL;

    node = be_alloc(BE_INT);
    vlog((!node), elog_be_alloc);
    retE_p((!node));

    node->val.i = num;
    return node;
}

static
struct be_node *be_create_addr(struct sockaddr_in* addr)
{
    struct be_node* node = NULL;
    char* s = NULL;
    int len = 2*sizeof(long) + 1;
    vassert(addr);

    node = be_alloc(BE_STR);
    vlog((!node), elog_be_alloc);
    retE_p((!node));

    s = (char*)malloc(len);
    vlog((!s), elog_malloc);
    ret1E_p((!s), be_free(node));
    memset(s, 0, len);

    ((long*)s)[0] = 8; //size
    ((long*)s)[2] = (long)addr->sin_port; // store port.
    ((uint32_t*)s)[1] = (uint32_t)addr->sin_addr.s_addr; // store ip.

    node->val.s = (char*)s + 4;
    return node;
}

static
int be_add_keypair(struct be_node *dict, char *str, struct be_node *node)
{
    struct be_dict* d = NULL;
    int len = strlen(str);
    char* s = NULL;
    int i = 0;

    vassert(dict);
    vassert(str);
    vassert(dict->type == BE_DICT);

    s = (char*)malloc(sizeof(long) + len + 1);
    vlog((!s), elog_malloc);
    retE((!s));

    set_int32(s, len);
    s = offset_addr(s, sizeof(long));
    memcpy(s, str, len);
    s[len] = '\0';

    for (; dict->val.d[i].val; i++);
    d = (struct be_dict*)realloc(dict->val.d, (i+2)*sizeof(*d));
    vlog((!d), elog_realloc);
    ret1E((!d), free(unoff_addr(s, sizeof(long))));

    dict->val.d = d;
    dict->val.d[i].key = s;
    dict->val.d[i].val = node;
    i++;
    dict->val.d[i].val = NULL;
    return 0;
}

static
struct be_node* be_create_info(vnodeInfo* info)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    int ret = 0;

    dict = be_create_dict();
    retE_p((!dict));

    node = be_create_str(info->id.data, VNODE_ID_LEN);
    ret1E_p((!node), be_free(dict));
    ret = be_add_keypair(dict, "id", node);
    ret2E_p((ret < 0), be_free(dict), be_free(node));

    node = be_create_addr(&info->addr);
    ret1E_p((!node), be_free(dict));
    ret = be_add_keypair(dict, "m", node);
    ret2E_p((ret < 0), be_free(dict), be_free(node));

    node = be_create_str(&info->ver, VNODE_ID_LEN);
    ret1E_p((!node), be_free(dict));
    ret = be_add_keypair(dict, "v", node);
    ret2E_p((ret < 0), be_free(dict), be_free(node));

    node = be_create_int(info->flags);
    ret1E_p((!node), be_free(dict));
    ret = be_add_keypair(dict, "f", node);
    ret2E_p((ret < 0), be_free(dict), be_free(node));

    return dict;
}

static
int be_add_list(struct be_node *list, struct be_node *node)
{
    struct be_node** l = NULL;
    int i = 0;

    vassert(list);
    vassert(node);
    vassert(list->type == BE_LIST);

    l = (struct be_node**)realloc(list->val.l, (i + 2)*sizeof(void*));
    vlog((!l), elog_realloc);
    retE((!l));
    list->val.l = l;

    for (; list->val.l[i]; i++);
    list->val.l[i] = node;
    i++;
    list->val.l[i] = NULL;
    return 0;
}

static
int be_encode(struct be_node *node, char *buf, int len)
{
    int off = 0;
    int ret = 0;

    vassert(node);
    vassert(buf);
    vassert(len > 0);

    switch(node->type) {
    case BE_STR: {
        long _len = get_int32(unoff_addr(node->val.s, sizeof(long)));
        ret = snprintf(buf+off, len-off, "%li", _len);
        retE((ret < 0));
        off += ret;
        ret = snprintf(buf+off, len-off, "%s", node->val.s);
        retE((ret < 0));
        off += ret;
        break;
    }
    case BE_INT:
        ret = snprintf(buf+off, len-off, "i%li", node->val.i);
        retE((ret < 0));
        off += ret;
        break;
    case BE_LIST: {
        int i = 0;
        ret = snprintf(buf+off, len-off, "l");
        retE((ret < 0));
        off += ret;
        for (; node->val.l[i]; i++) {
            ret = be_encode(node->val.l[i], buf+off, len-off);
            retE((ret < 0));
            off += ret;
        }
        ret = snprintf(buf+off, len-off, "e");
        retE((ret < 0));
        off += ret;
    }
    case BE_DICT: {
        int i = 0;
        ret = snprintf(buf+off, len-off, "d");
        retE((ret < 0));
        off += ret;
        for (i = 0; node->val.d[i].val; i++) {
            char* _key = node->val.d[i].key;
            long  _len = get_int32(unoff_addr(_key, sizeof(long)));
            ret = snprintf(buf + off, len - off, "%li:%s", _len, _key);
            retE((ret < 0));
            off += ret;

            ret = be_encode(node->val.d[i].val, buf+off, len-off);
            retE((ret < 0));
            off += ret;
        }
        ret = snprintf(buf+off, len-off, "e");
        retE((ret < 0));
        off += ret;
        break;
    }
    default:
        return -1;
        break;
    }
    return off;
}


/*
 * @token:
 * @srcId: Id of node sending query.
 * @buf:
 * @len:
 *
 * ping Query = {"t":"aa",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"abcdefghij0123456789"}}
 * bencoded = d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe
 */
static
int _vdht_enc_ping(vtoken* token, vnodeId* srcId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* id   = NULL;
    char* str_q = "q";
    char* ping = "ping";
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str(str_q, strlen(str_q));
    be_add_keypair(dict, "y", node);

    node = be_create_str(ping, strlen(ping));
    be_add_keypair(dict, "q", node);

    id   = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(id, "id", node);
    be_add_keypair(dict,"a", id);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    return ret;
}

/*
 * @token:
 * @srcId: Id of node replying query.
 * @result: queried result
 * @buf:
 * @len:
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
int _vdht_enc_ping_rsp(vtoken* token, vnodeId* srcId,vnodeInfo* result,void* buf, int   sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    char* str_r = "r";
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str(str_r, strlen(str_r));
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(rslt, "id", node);
    node = be_create_info(result);
    be_add_keypair(rslt, "node", node);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
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
    struct be_node* a    = NULL;
    char* str_q = "q";
    char* query = "find_node";
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str(str_q, strlen(str_q));
    be_add_keypair(dict, "y", node);

    node = be_create_str(query, strlen(query));
    be_add_keypair(dict, "q", node);

    a = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(a, "id", node);
    node = be_create_str(targetId->data, VNODE_ID_LEN);
    be_add_keypair(a, "target", node);
    be_add_keypair(dict, "a", a);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
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
    char* str_r = "r";
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str(str_r, strlen(str_r));
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(rslt, "id", node);
    node = be_create_info(result);
    be_add_keypair(rslt, "node", node);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
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
    struct be_node* a    = NULL;
    char* str_q = "q";
    char* query = "find_closest_nodes";
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str(str_q, strlen(str_q));
    be_add_keypair(dict, "y", node);

    node = be_create_str(query, strlen(query));
    be_add_keypair(dict, "q", node);

    a = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(a, "id", node);
    node = be_create_str(targetId->data, VNODE_ID_LEN);
    be_add_keypair(a, "target", node);
    be_add_keypair(dict, "a", a);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
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
    char* str_r = "r";
    int ret = 0;
    int i   = 0;

    vassert(token);
    vassert(srcId);
    vassert(closest);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str(str_r, strlen(str_r));
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(rslt, "id", node);

    list = be_create_list();
    for (; i < varray_size(closest); i++) {
        vnodeInfo* info = (vnodeInfo*)varray_get(closest, i);
        node = be_create_info(info);
        be_add_list(list, node);
    }
    be_add_keypair(rslt, "nodes", list);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
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
struct be_node* _aux_get_dict(struct be_node* node, char* key)
{
    int found = 0;
    int i = 0;

    vassert(node);
    vassert(key);

    retE_p((node->type != BE_DICT));

    for(; node->val.d[i].val; i++) {
        if (!strcmp(key, node->val.d[i].key)) {
            found = 1;
            break;
        }
    }
    retE_p((!found));
    return node->val.d[i].val;
}

static
struct be_node* _aux_get_node(struct be_node* dict, char* key1, char* key2)
{
    struct be_node* node = NULL;
    vassert(dict);
    vassert(key1);
    vassert(key2);

    node = _aux_get_dict(dict, key1);
    retE_p((!node));
    retE_p((BE_DICT != node->type));

    node = _aux_get_dict(node, key2);
    retE_p((!node));
    return node;
}

static
int _aux_get_token(struct be_node* dict, vtoken* token)
{
    struct be_node* node = NULL;
    int len = 0;
    vassert(node);
    vassert(token);

    node = _aux_get_dict(dict, "t");
    retE((!node));
    retE((node->type != BE_STR));

    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != VTOKEN_LEN));

    memcpy(&token->data, node->val.s, len);
    return 0;
}

static
int _aux_get_id(struct be_node* dict, char* key1, char* key2, vnodeId* id)
{
    struct be_node* node = NULL;
    int len = 0;

    vassert(dict);
    vassert(key1);
    vassert(key2);
    vassert(id);

    node = _aux_get_node(dict, key1, key2);
    retE((!node));
    retE((BE_STR != node->type));

    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((VNODE_ID_LEN != len));
    memcpy(id->data, node->val.s, len);

    return 0;
}

static
int _aux_get_info(struct be_node* dict, vnodeInfo* info)
{
    struct be_node* node = NULL;
    int len = 0;
    vassert(dict);
    vassert(info);

    node = _aux_get_dict(dict, "id");
    retE((!node));
    retE((BE_STR != node->type));
    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != VNODE_ID_LEN));
    memcpy(info->id.data, node->val.s, len);

    node = _aux_get_dict(dict, "m"); // address.
    retE((!node));
    retE((BE_STR != node->type));
    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != 8));

    info->addr.sin_family = AF_INET;
    info->addr.sin_addr.s_addr = get_uint32(node->val.s);
    info->addr.sin_port = get_int32(unoff_addr(node->val.s, sizeof(uint32_t)));

    node = _aux_get_dict(dict, "v"); // version.
    retE((!node));
    retE((BE_STR != node->type));
    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != VNODE_ID_LEN));
    memcpy(info->ver.data, node->val.s, len);

    node = _aux_get_dict(dict, "f");  // flags
    retE((!node));
    retE((BE_INT != node->type));
    info->flags = (uint32_t)node->val.i;

    return 0;
}

static
int _aux_get_mtype(struct be_node* dict, int* mtype)
{
    struct be_node* node = NULL;

    vassert(dict);
    vassert(mtype);

    node = _aux_get_dict(dict, "y");
    retE((!node));
    retE((BE_STR != node->type));

    if (!strcmp(node->val.s, "q")) {
        node = _aux_get_dict(node, "q");
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
        node = _aux_get_dict(node, "r");
        retE((!node));
        retE((BE_STR != node->type));
        tmp = _aux_get_dict(node, "hash");
        if (tmp) {
            *mtype = VDHT_GET_PEERS_R;
            return 0;
        }
        tmp = _aux_get_dict(node, "node");
        if (tmp) {
            *mtype = VDHT_FIND_NODE_R;
            return 0;
        }
        tmp = _aux_get_dict(node, "nodes");
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
 * ping Query = {"t":"aa",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"abcdefghij0123456789"}}
 * bencoded = d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe
 */
static
int _vdht_dec_ping(void* ctxt, vtoken* token, vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret  = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);

    ret = _aux_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_get_id(dict, "a", "id", srcId);
    retE((ret < 0));

    return 0;
}

/*
 * @buf:
 * @sz:
 * @token:
 * @result:
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
int _vdht_dec_ping_rsp(void* ctxt, vtoken* token, vnodeId* srcId, vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(srcId);
    vassert(result);

    ret = _aux_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_get_id(dict, "r", "id", srcId);
    retE((ret < 0));

    node = _aux_get_node(dict, "r", "node");
    retE((!node));
    ret  = _aux_get_info(node, result);
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

    ret = _aux_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_get_id(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_get_id(dict, "a", "target", targetId);
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

    ret = _aux_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_get_id(dict, "r", "id", srcId);
    retE((ret < 0));

    node = _aux_get_node(dict, "r", "node");
    retE((!node));
    ret  = _aux_get_info(node, result);
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

    ret = _aux_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_get_id(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_get_id(dict, "a", "target", targetId);
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

    ret = _aux_get_token(dict, token);
    retE((ret < 0));
    ret = _aux_get_id(dict, "r", "id", srcId);
    retE((ret < 0));

    list = _aux_get_node(dict, "r", "nodes");
    retE((ret < 0));
    retE((BE_LIST != list->type));

    for (; node->val.l[i]; i++) {
        vnodeInfo* info = NULL;

        node = list->val.l[i];
        retE((BE_DICT != node->type));
        info = vnodeInfo_alloc();
        retE((!info));

        ret = _aux_get_info(node, info);
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

    dict = be_decode(buf, len);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = _aux_get_mtype(dict, &mtype);
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

