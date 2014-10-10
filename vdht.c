#include "vglobal.h"
#include "vdht.h"

enum {
    Q_PING,
    Q_FIND_NODE,
    Q_GET_PEERS,
    Q_ANNONCE_PEER,
    Q_FIND_CLOSEST_NODES,
    Q_UNKOWN,
    R_PING,
    R_FIND_NODE,
    R_GET_PEERS,
    R_ANNOUNCE_PEER,
    R_FIND_CLOSEST_NODES,
    R_UNKOWN
};
char* be_queries[Q_UNKOWN] = {
    "ping",
    "find_node",
    "get_peers",
    "announce_peer",
    "find_closest_nodes",
    NULL
};

enum {
    BE_STR,
    BE_INT,
    BE_LIST,
    BE_DICT,
    BE_BUTT
};

struct be_node;
struct be_dict {
    char* key;
    struct be_node* val;
};

struct be_node {
    int type;
    union {
        char* s;
        long long i;
        struct be_node **l;
        struct be_dict *d;
    }val;
};

#define LL(addr)          (*(long long*)addr)
#define SET_LL(addr, val) (*(long long*)addr = (long long)val)
#define OFF_LL(addr)      (addr + sizeof(long long))
#define UNOFF_LL(addr)    (addr - sizeof(long long))

static
be_node* be_alloc(int type)
{
	be_node *node = NULL;

    vassert(type >= BE_STR);
	vassert(type < BE_BUTT);

	node = (be_node *) malloc(sizeof(*node));
	ret1E_p((!node), elog_malloc);

	memset(node, 0, sizeof(*node));
	node->type = type;
	return node;
}

static
void be_free(be_node *node)
{
    vassert(node);

    switch (node->type) {
    case BE_STR:
        free(UNOFF_LL(node->val.s));
        break;
    case BE_INT:
        break;
    case BE_LIST: {
        int i = 0;
        for (; node->val.l[i]; i++) {
            be_free(node->val.l[i]);
        }
        free(node->val.l);
        break;
    }
    case BE_DICT: {
        int i = 0;
        for (; node->val.d[i].val; i++) {
            free(UNOFF_LL(node->val.d[i].key));
            be_free(node->val.d[i].val);
        }
        free(node->val.d);
        break;
    }
    default:
        vassert(0);
    }
	free(node);
}

static
long long _be_decode_int(const char **data, long long *data_len)
{
	char *endp;
	long long ret = strtoll(*data, &endp, 10);
	*data_len -= (endp - *data);
	*data = endp;
	return ret;
}

static char *_be_decode_str(const char **data, long long *data_len)
{
	long long sllen = 0;
	long slen = sllen;
	unsigned long len;
	char *ret = NULL;

    sllen = _be_decode_int(data, data_len);
    retE_p((sllen < 0));
    slen = (long)sllen;
    retE_p((sllen != (long long)slen));
    retE_p((sllen > *data_len - 1));

	/* switch from signed to unsigned so we don't overflow below */
	len = slen;

	if (**data == ':') {
		char *_ret = (char *) malloc(sizeof(sllen) + len + 1);
		SET_LL(ret, sllen);
		ret = _ret + sizeof(sllen);
		memcpy(ret, *data + 1, len);
		ret[len] = '\0';
		*data += len + 1;
		*data_len -= len + 1;
	}
	return ret;
}

static
be_node *_be_decode(const char **data, long long *data_len)
{
	be_node *ret = NULL;

	vassert(*data);
	vassert(data_len);
	vassert(*data_len > 0);

	switch (**data) {
	case 'l': { //list
		int i = 0;
		ret = be_alloc(BE_LIST);

		--(*data_len);
		++(*data);
		while (**data != 'e') {
			ret->val.l = (be_node **) realloc(ret->val.l, (i + 2) * sizeof(be_node**));
			ret->val.l[i] = _be_decode(data, data_len);
			ret1E_p((!ret->val.l[i]), be_free(ret));
			++i;
		}
		--(*data_len);
		++(*data);

		/* empty list case. */
		if (i == 0)	{
			ret->val.l = (be_node **) realloc(ret->val.l, 1 * sizeof(*ret->val.l));
		}
		ret->val.l[i] = NULL;
		return ret;
	}
	case 'd': { /* dictionaries */
	    int i = 0;
		ret = be_alloc(BE_DICT);

		--(*data_len);
		++(*data);
		while (**data != 'e') {
			ret->val.d = (be_dict *) realloc(ret->val.d, (i + 2) * sizeof(*ret->val.d));
			ret->val.d[i].key = _be_decode_str(data, data_len);
			ret->val.d[i].val = _be_decode(data, data_len);
			if ((!ret->val.d[i].key) || (!ret->val.d[i].val)) {
				/* failed decode - kill decode */
				be_free(ret);
				return NULL;
			}
			++i;
		}
		--(*data_len);
		++(*data);

		/* empty dictionary case. */
		if (i == 0)	{
			ret->val.d = (be_dict *) realloc(ret->val.d, 1 * sizeof(*ret->val.d));
		}
		ret->val.d[i].val = NULL;
		return ret;
	}
	case 'i': { /* integers */
		ret = be_alloc(BE_INT);
		--(*data_len);
		++(*data);
		ret->val.i = _be_decode_int(data, data_len);
		if (**data != 'e') {
			be_free(ret);
			return NULL;
		}
		--(*data_len);
		++(*data);

		return ret;
	}
	case '0'...'9': { /* byte strings */
		ret = be_alloc(BE_STR);
		ret->val.s = _be_decode_str(data, data_len);
		return ret;
	}
	default: /* invalid*/
		return NULL;
		break;
	}

	return ret;
}

be_node *be_decode(const char *data, long long len)
{
	return _be_decode(&data, &len);
}

be_node *be_decode_str(const char *data)
{
	return be_decode(data, strlen(data));
}

/* hackish way to create nodes! */
static
be_node *be_create_dict()
{
    return be_decode_str("de");
}

static
be_node *be_create_list()
{
    return be_decode_str("le");
}

static
be_node *be_create_str(const char* str, int len)
{
    be_node* n = NULL;
    char*    s = NULL;
    vassert(str);
    vassert(len > 0);

    s = (char*)malloc(sizeof(long long) + len + 1);
    ret1E_p((!s), free(s));
    SET_LL(s, len);
    strncpy(OFF_LL(s), str, len);

    n = be_alloc(BE_STR);
    retE_p((!n));

    n->val.s = OFF_LL(s);
    return n;
}

static
be_node *be_create_int(long long int num)
{
    be_node *n = NULL;

    n = be_alloc(BE_INT);
    retE_p((!n));

    n->val.i = num;
    return n;
}

static
be_node* be_create_info_node(vnodeInfo* info)
{
    //todo;
    return NULL;
}

static
int be_add_keypair(be_node *dict, const char *str, be_node *node)
{
    be_dict* d = NULL;
    char*    s = NULL;
    int len = strlen(str);
    int i = 0;

    vassert(dict);
    vassert(str);
    vassert(dict->type == BE_DICT);

    s = (char*)malloc(sizeof(long long) + len + 1);
    ret1E_p((!s), elog_malloc);
    SET_LL(s, len);
    strncpy(OFF_LL(s), str, len);

    for (; dict->val.d[i].val; i++);
    d = (be_dict*)realloc(dict->val.d, (i+2)*sizeof(be_dict));
    ret1E_p((!d), free(s));

    dict->val.d = d;
    dict->val.d[i].key = OFF_LL(s);
    dict->val.d[i].val = node;
    i++;
    dict->val.d[i].val = NULL;
    return 0;
}

static
int be_add_list(be_node *list, be_node *node)
{
    be_node** l = NULL;

    int i = 0;
    vassert(list->type == BE_LIST);

    vassert(list);
    vassert(node);

    l = (be_node**)realloc(list->val.l, (i + 2)*sizeof(be_node));
    ret1E_p((!l), elog_realloc);

    for (; list->val.l[i]; i++);
    list->val.l[i] = node;
    i++;
    list->val.l[i] = NULL;
    return 0;
}

static
long long _be_str_len(be_node *node)
{
    long long ret = 0;
    vassert(node);

    if (node->val.s) {
        ret = LL(UNOFF_LL(node->val.s));
    }
    return ret;
}

static
int be_encode(be_node *node, char *buf, int len)
{
    int off = 0;
    int ret = 0;

    switch(node->type) {
    case BE_STR:
        ret = snprintf(buf+off, len-off, "%lli", _be_str_len(node));
        retE((ret < 0));
        off += ret;
        ret = snprintf(buf+off, len-off, "%s",   node->val.s);
        retE((ret < 0));
        off += ret;
        break;
    case BE_INT:
        ret = snprintf(buf+off, len-off, "i%llie", node->val.i);
        retE((ret < 0));
        off += ret;
        break;
    case BE_LIST: {
        int i = 0;
        ret = snprintf(buf+off, len-off, "l");
        retE((ret < 0));
        off += ret;
        for (; node->val.l[i]; i++) {
            ret = be_encode(node->val.l[i], buf + off, len-off);
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
            char* key = node->val.d[i].key;
            ret = snprintf(buf + off, len - off, strlen(key), key);
            retE((ret < 0));
            off += ret;
        }
        ret = snprintf(buf+off, len-off, "e");
        ret((ret < 0));
        off += ret;
        break;
    }
    default:
        vassert(0);
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
int _vdht_enc_ping(
        vtoken* token,
        vnodeId* srcId,
        void* buf,
        int sz)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* id   = NULL;
    char* ping = be_queries[PING];
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VNODE_ID_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q", strlen("q"));
    be_add_keypair(dict, "y", node);

    node = be_create_str(ping, strlen(ping));
    be_add_keypair(dict, "q", node);

    node = be_create_str(srcId->data, VNODE_ID_LEN);
    id   = be_create_dict();
    be_add_keypair(id, "id", node);
    be_add_keypair(dict, "a", id);

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
int _vdht_enc_ping_rsp(
        vtoken* token,
        vnodeId* srcId,
        vnodeInfo* result,
        void* buf,
        int   sz)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* rslt = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r", strlen("r"));
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(rslt, "id", node);
    node = be_create_info_node(result);
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
int _vdht_enc_find_node(
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId,
        void* buf,
        int   sz)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* a    = NULL;
    char* method  = be_queries[FIND_NODE];
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q", strlen("q"));
    be_add_keypair(dict, "y", node);

    node = be_create_str(method, strlen(method));
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
int _vdht_enc_find_node_rsp(
        vtoken* token,
        vnodeId* srcId,
        vnodeInfo* result,
        void* buf,
        int sz)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* rslt = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r", strlen("r"));
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(rslt, "id", node);
    node = be_create_info_node(result);
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
int _vdht_enc_get_peers(
        vtoken* token,
        vnodeId* srcId,
        vnodeHash* hash,
        void* buf,
        int sz)
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
int _vdht_enc_find_closest_nodes(
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId,
        void* buf,
        int sz)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* a    = NULL;
    char* query = be_queries[FIND_CLOSEST_NODES];
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    node = be_create_str(token->data, VTOKEN_LEN);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q", strlen("q"));
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

    //todo
    return 0;
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* rslt = NULL;
    be_node* list = NULL;
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

    node = be_create_str("r", strlen("r"));
    be_add_keypair(dict, "y", node);

    rslt = be_create_dict();
    node = be_create_str(srcId->data, VNODE_ID_LEN);
    be_add_keypair(rslt, "id", node);

    list = be_create_list();
    for (; i < varray_size(closest); i++) {
        vnodeInfo* info = (vnodeInfo*)varray_get(closest, i);
        node = be_create_info_node(info);
        be_add_list(list, node);
    }
    be_add_keypair(rslt, "nodes", list);
    be_add_keypair(dict, "r", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    return ret;

    //tdoo;
    return 0;
}

struct vdht_enc_ops dht_enc_ops = {
    .ping               = _vdht_enc_ping,
    .ping_rsp           = _vdht_enc_ping_rsp,
    .find_node          = _vdht_enc_find_node,
    .find_node_rsp      = _vdht_enc_find_node_rsp,
    .get_peers          = _vdht_enc_get_peers,
    .get_peers_rsp      = _vdht_enc_get_peers_rsp,
    .find_closest_nodes = _vdht_enc_find_closest_nodes,
    .find_closest_nodes_rsp = _vdht_enc_find_closest_nodes_rsp
};

static
be_node* _be_get_dict(be_node* node, const char* key)
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

enum {
    Y_Q,
    Y_R,
    Y_UNKOWN,
    Y_BUTT,
};

int be_get_y(be_node* node)
{
    be_node* val = NULL;
    int type = Y_UNKOWN;

    vassert(node);

    val = _be_get_dict(node, "y");
    retE((!val));
    retE((val->type != BE_STR));

    switch(val->val.s[0]) {
    case 'q':
        type = Y_Q;
        break;
    case 'r':
        type = Y_R;
        break;
    default:
        type = Y_UNKOWN;
    }
    return type;
}

int be_get_q(be_node* node)
{
    be_node* val = NULL;
    int type = Q_UNKOWN;
    vassert(node);

    val = _be_get_dict(node, "q");
    retE((!val));
    retE((val->type != BE_STR));

    if (!strcmp(val->val.s, "ping")) {
        type = Q_PING;
    }else if (!strcmp(val->val.s, "find_node")) {
        type = Q_FIND_NODE;
    }else if (!strcmp(val->val.s, "get_peers")) {
        type = Q_GET_PEERS;
    }else if (!strcmp(val->val.s, "find_closest_nodes")) {
        type = Q_FIND_CLOSEST_NODES;
    }else {
        type = Q_UNKOWN;
    }
    return type;
}

be_node* be_get_a(be_node* node)
{
    vassert(node);

    //todo;
    return NULL;
}

int be_get_r(be_node* node)
{
    be_node* val = NULL;
    be_node* id  = NULL;
    int type = R_UNKOWN;

    val = _be_get_dict(node, "r");
    retE((!val));
    retE((val->type != BE_DICT));

    id = _be_get_dict(val, "id");



}

int be_get_t(be_node* node, vtoken* token)
{
    vassert(node);
    vassert(token);

    //todo;
    return 0;
}

int be_get_id(be_node* node, vnodeId* id)
{
    vassert(node);
    vassert(id);

    //todo;
    return 0;
}

int be_get_mtype(be_node *node)
{
    int y = 0;

    vassert(node);
    y = _be_get_y(node); // check for y: q or r.
    retE((y < 0));
    retE((y == Y_UNKOWN));

    switch(y) {
    case Y_Q:
        be_node* query = _be_get_q();
        break;
    case Y_R:
        be_node* reply = _be_get_r();

    }


}

int be_get_node_info(be_node* node)
{
    vassert(node);

    //todo;
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
int _vdht_dec_ping(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    int ret  = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);

    dict = be_decode(buf, sz);
    retE((!dict));

    ret = be_get_mtype(dict);
    ret1E((ret != Q_PING), be_free(dict));

    ret = be_get_t(dict, token);
    ret1E((ret < 0), be_free(dict));

    node = be_get_a(dict);
    ret((!node), be_free(dict));
    ret = _be_get_id(node, srcId);
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
int _vdht_dec_ping_rsp(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId,
        vnodeInfo* result)
{
    be_node* dict = NULL;
    be_node* node = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(result);

    dict = be_decode(buf, sz);
    retE((!dict));

    ret = be_get_mtype(dict);
    retE((ret < R_PING),      be_free(dict));
    retE((ret > R_FIND_NODE), be_free(dict));

    ret = be_get_t(dict, token);
    retE((ret < 0), be_free(dict));

    node = be_get_r(dict);
    retE((!node), be_free(dict));

    ret = _be_get_id(node, srcId);
    retE((ret < 0), be_free(dict));

    node = _be_get_dict(node, "node");
    retE((!node), be_free(dict));

    ret = _be_get_node_info(node, result);
    retE((ret < 0), be_free(dict));

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
int _vdht_dec_find_node(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId)
{
    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

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
int _vdht_dec_find_node_rsp(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId,
        vnodeInfo* result)
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
 * @hash:
 */
static
int _vdht_dec_get_peers(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId,
        vnodeHash* hash)
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
int _vdht_dec_get_peers_rsp(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId,
        struct varray* result)
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
int _vdht_dec_find_closest_nodes(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId)
{
    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);

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
int _vdht_dec_find_closest_nodes_rsp(
        void* buf,
        int sz,
        vtoken* token,
        vnodeId* srcId,
        struct varray* closest)
{
    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(closest);

    //todo;
    return 0;
}

static
struct vdht_dec_ops dht_dec_ops = {
    .ping              = _vdht_dec_ping,
    .ping_rsp          = _vdht_dec_ping_rsp,
    .find_node         = _vdht_dec_find_node,
    .find_node_rsp     = _vdht_dec_find_node_rsp,
    .get_peers         = _vdht_dec_get_peers,
    .get_peers_rsp     = _vdht_dec_get_peers_rsp,
    .find_closet_nodes = _vdht_dec_find_closest_nodes,
    .find_cloest_nodes_rsp = _vdht_dec_find_closest_nodes_rsp
};

