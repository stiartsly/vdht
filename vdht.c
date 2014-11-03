#include "vglobal.h"
#include "vdht.h"

enum {
    Q_PING,
    Q_FIND_NODE,
    Q_GET_PEERS,
    Q_POST_PEER,
    Q_FIND_CLOSEST_NODES,
    Q_UNKOWN,
    R_PING,
    R_FIND_NODE,
    R_GET_PEERS,
    R_POST_PEER,
    R_FIND_CLOSEST_NODES,
    R_UNKOWN
};

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
		if (i == 0)	{
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
int _enc_ping(vtoken* token, vnodeId* srcId, void* buf, int sz)
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
int _enc_ping_rsp(vtoken* token, vnodeId* srcId,vnodeInfo* result,void* buf, int   sz)
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
int _enc_find_node(vtoken* token, vnodeId* srcId, vnodeId* targetId, void* buf,int sz)
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
int _enc_find_node_rsp(vtoken* token, vnodeId* srcId, vnodeInfo* result, void* buf,int sz)
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
int _enc_get_peers(vtoken* token,vnodeId* srcId, vnodeHash* hash, void* buf, int sz)
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
int _enc_get_peers_rsp(
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
int _enc_find_closest_nodes(vtoken* token, vnodeId* srcId, vnodeId* targetId, void* buf, int sz)
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
int _enc_find_closest_nodes_rsp(
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
    .ping                   = _enc_ping,
    .ping_rsp               = _enc_ping_rsp,
    .find_node              = _enc_find_node,
    .find_node_rsp          = _enc_find_node_rsp,
    .get_peers              = _enc_get_peers,
    .get_peers_rsp          = _enc_get_peers_rsp,
    .find_closest_nodes     = _enc_find_closest_nodes,
    .find_closest_nodes_rsp = _enc_find_closest_nodes_rsp
};

static
struct be_node* be_get_dict(struct be_node* node, char* key)
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
struct be_node* be_get_node(struct be_node* dict, char* key1, char* key2)
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
int be_check_mtype(struct be_node* dict, int type)
{
    struct be_node* node = NULL;
    char* query = NULL;

    node = be_get_dict(dict, "q");
    retE((!node));
    retE((BE_STR != node->type));

    switch (type) {
    case Q_PING:
        query = "ping";
        break;
    case Q_FIND_NODE:
        query = "find_node";
        break;
    case Q_GET_PEERS:
        query = "get_peers";
        break;
    case Q_FIND_CLOSEST_NODES:
        query = "find_closest_nodes";
        break;
    default:
        break;
    }

    if (query && !strcmp(node->val.s, query)) {
        // means query msg.
        return 0;
    }

    // otherwise, must be dht response msg.
    node = be_get_node(dict, "r", "node");
    if (node) {
        retE((type != R_PING && type != R_FIND_NODE));
        return 0;
    }
    node = be_get_node(dict, "r", "nodes");
    if (node) {
        retE((type == R_FIND_CLOSEST_NODES));
        return 0;
    }
    //TODO: for hash
    return -1;
}

static
int be_get_token(struct be_node* dict, vtoken* token)
{
    struct be_node* node = NULL;
    int len = 0;
    vassert(node);
    vassert(token);

    node = be_get_dict(dict, "t");
    retE((!node));
    retE((node->type != BE_STR));

    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != VTOKEN_LEN));

    memcpy(&token->data, node->val.s, len);
    return 0;
}

int be_get_id(struct be_node* dict, char* key1, char* key2, vnodeId* id)
{
    struct be_node* node = NULL;
    int len = 0;

    vassert(dict);
    vassert(key1);
    vassert(key2);
    vassert(id);

    node = be_get_dict(dict, key1);
    retE((!node));
    retE((BE_DICT != node->type));

    node = be_get_dict(node, key2);
    retE((!node));
    retE((BE_STR  != node->type));

    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((VNODE_ID_LEN != len));
    memcpy(id->data, node->val.s, len);

    return 0;
}

int be_get_info(struct be_node* dict, vnodeInfo* info)
{
    struct be_node* node = NULL;
    int len = 0;
    vassert(dict);
    vassert(info);

    node = be_get_dict(dict, "id");
    retE((!node));
    retE((node->type != BE_STR));
    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != VNODE_ID_LEN));
    memcpy(info->id.data, node->val.s, len);

    node = be_get_dict(dict, "m");
    retE((!node));
    retE((node->type != BE_STR));
    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != 8));

    info->addr.sin_family = AF_INET;
    info->addr.sin_addr.s_addr = get_uint32(node->val.s);
    info->addr.sin_port = get_int32(unoff_addr(node->val.s, sizeof(uint32_t)));

    node = be_get_dict(dict, "v");
    retE((!node));
    retE((node->type != BE_STR));
    len = get_int32(unoff_addr(node->val.s, sizeof(long)));
    retE((len != VNODE_ID_LEN));
    memcpy(info->ver.data, node->val.s, len);

    node = be_get_dict(dict, "f");
    retE((!node));
    retE((node->type != BE_INT));
    info->flags = (uint32_t)node->val.i;

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
int _dec_ping(void* buf, int sz, vtoken* token, vnodeId* srcId)
{
    struct be_node* dict = NULL;
    int ret  = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);

    dict = be_decode(buf, sz);
    retE((!dict));

    ret = be_check_mtype(dict, Q_PING);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_token(dict, token);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "a", "id", srcId);
    ret1E((ret < 0), be_free(dict));

    be_free(dict);
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
int _dec_ping_rsp(void* buf, int sz, vtoken* token, vnodeId* srcId, vnodeInfo* result)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(result);

    dict = be_decode(buf, sz);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = be_check_mtype(dict, R_PING);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_token(dict, token);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "r", "id", srcId);
    ret1E((ret < 0), be_free(dict));

    node = be_get_node(dict, "r", "node");
    ret1E((!node), be_free(dict));
    ret  = be_get_info(node, result);
    ret1E((ret < 0), be_free(dict));

    be_free(dict);
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
int _dec_find_node(void* buf, int sz, vtoken* token, vnodeId* srcId, vnodeId* targetId)
{
    struct be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    dict = be_decode(buf, sz);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = be_check_mtype(dict, Q_FIND_NODE);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_token(dict, token);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "a", "id", srcId);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "a", "target", targetId);
    ret1E((ret < 0), be_free(dict));

    be_free(dict);
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
int _dec_find_node_rsp(void* buf, int sz,vtoken* token, vnodeId* srcId, vnodeInfo* result)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(result);

    dict = be_decode(buf, sz);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = be_check_mtype(dict, R_FIND_NODE);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_token(dict, token);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "r", "id", srcId);
    ret1E((ret < 0), be_free(dict));

    node = be_get_node(dict, "r", "node");
    ret1E((!node), be_free(dict));
    ret  = be_get_info(node, result);
    ret1E((ret < 0), be_free(dict));

    be_free(dict);
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
int _dec_get_peers(void* buf, int sz, vtoken* token, vnodeId* srcId, vnodeHash* hash)
{
    vassert(buf);
    vassert(sz > 0);
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
int _dec_get_peers_rsp(void* buf, int sz, vtoken* token, vnodeId* srcId, struct varray* result)
{
    vassert(buf);
    vassert(sz > 0);
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
int _dec_find_closest_nodes(void* buf, int sz, vtoken* token, vnodeId* srcId, vnodeId* targetId)
{
    struct be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    dict = be_decode(buf, sz);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = be_check_mtype(dict, Q_FIND_CLOSEST_NODES);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_token(dict, token);
    ret1E((ret < 0), be_free(dict));

    ret = be_get_id(dict, "a", "id", srcId);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "a", "target", targetId);
    ret1E((ret < 0), be_free(dict));

    be_free(dict);
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
int _dec_find_closest_nodes_rsp(void* buf, int sz, vtoken* token, vnodeId* srcId, struct varray* closest)
{
    struct be_node* dict = NULL;
    struct be_node* list = NULL;
    struct be_node* node = NULL;
    int ret = 0;
    int i = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(closest);

    dict = be_decode(buf, sz);
    vlog((!dict), elog_be_decode);
    retE((!dict));

    ret = be_check_mtype(dict, R_FIND_CLOSEST_NODES);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_token(dict, token);
    ret1E((ret < 0), be_free(dict));
    ret = be_get_id(dict, "r", "id", srcId);
    ret1E((ret < 0), be_free(dict));

    list = be_get_node(dict, "r", "nodes");
    ret1E((!list), be_free(dict));
    ret1E((BE_LIST != list->type), be_free(dict));

    for (; node->val.l[i]; i++) {
        vnodeInfo* info = NULL;

        node = list->val.l[i];
        ret1E((BE_DICT != node->type), be_free(dict));
        info = vnodeInfo_alloc();
        ret1E((!info), be_free(dict));

        ret = be_get_info(node, info);
        if (ret < 0) {
            vnodeInfo_free(info);
            be_free(dict);
            return -1;
        }
        varray_add_tail(closest, info);
    }

    be_free(dict);
    return 0;
}

struct vdht_dec_ops dht_dec_ops = {
    .ping                   = _dec_ping,
    .ping_rsp               = _dec_ping_rsp,
    .find_node              = _dec_find_node,
    .find_node_rsp          = _dec_find_node_rsp,
    .get_peers              = _dec_get_peers,
    .get_peers_rsp          = _dec_get_peers_rsp,
    .find_closest_nodes     = _dec_find_closest_nodes,
    .find_closest_nodes_rsp = _dec_find_closest_nodes_rsp
};

