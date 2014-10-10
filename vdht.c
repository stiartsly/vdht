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

static
be_node* be_alloc(int type)
{
	be_node *node = NULL;

    vassert(type >= BE_STR);
	vassert(type < BE_BUTT);

	node = (be_node *) malloc(sizeof(*node));
	vlog_cond((!node), elog_malloc);
	retE_p((!node));

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
        free(UNOFF_INT32(node->val.s));
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
            free(UNOFF_INT32(node->val.d[i].key));
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
int _be_decode_int(char** data, int* data_len)
{
	char *endp;
	long ret = strtol(*data, &endp, 10);
	*data_len -= (endp - *data);
	*data = endp;
	return (int)ret;
}

static
char *_be_decode_str(const char **data, long long *data_len)
{
	char* ret = NULL;
	int len = 0;

    len = _be_decode_int(data, data_len);
    retE_p((len < 0));
    retE_p((len < *data_len - 1));

	if (**data == ':') {
	    char* _ret = (char*) malloc(sizeof(len) + len + 1);
	    SET_INT32(_ret, len);
	    ret = OFF_INT32(_ret);
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

static
be_node *be_decode(const char *data, long long len)
{
	return _be_decode(&data, &len);
}

static
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

    s = (char*)malloc(sizeof(long) + len + 1);
    vlog_cond((!s), elog_malloc);
    retE_p((!s));

    SET_INT32(s, len);
    strncpy(OFF_INT32(s), str, len);

    n = be_alloc(BE_STR);
    vlog_cond((!n), elog_be_alloc);
    ret1E_p((!n), free(s));

    n->val.s = OFF_INT32(s);
    return n;
}

static
be_node *be_create_int(long long num)
{
    be_node *n = NULL;

    n = be_alloc(BE_INT);
    vlog_cond((!n), elog_be_alloc);
    retE_p((!n));

    n->val.i = num;
    return n;
}

static
be_node *be_create_addr(struct sockaddr_in* addr)
{
    //todo;
    vassert(0);
    return NULL;
}

static
be_node* be_create_info(vnodeInfo* info)
{
    be_node* dict = NULL;
    be_node* node = NULL;

    dict = be_create_dict();
    node = be_create_str(&info->id, VNODE_ID_LEN);
    be_add_keypair(dict, "id", node);

    node = be_create_addr(&info->addr);
    be_add_keypair(dict, "m", node);

    node = be_create_str(&info->ver, VNODE_ID_LEN);
    be_add_keypair(dict, "v", node);

    node = be_create_int(info->flags);
    be_add_keypair(dict, "f", node);

    return dict;
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
    vlog_cond((!s), elog_malloc);
    retE((!s));

    SET_INT32(s, len);
    memcpy(OFF_INT32(s), str, len);

    for (; dict->val.d[i].val; i++);
    d = (be_dict*)realloc(dict->val.d, (i+2)*sizeof(be_dict));
    vlog_cond((!d), elog_realloc);
    retE((!d), free(s));


    dict->val.d = d;
    dict->val.d[i].key = OFF_INT32(s);
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

    l = (be_node**)realloc(list->val.l, (i + 2)*sizeof(be_node*));
    vlog_cond((!l), elog_realloc);
    retE((!l));
    list->val.l = l;

    for (; list->val.l[i]; i++);
    list->val.l[i] = node;
    i++;
    list->val.l[i] = NULL;
    return 0;
}

static
int be_encode(be_node *node, char *buf, int len)
{
    int off = 0;
    int ret = 0;

    vassert(node);
    vassert(buf);
    vassert(len > 0);

    switch(node->type) {
    case BE_STR:
        ret = snprintf(buf+off, len-off, "%lli", INT32(UNOFF_INT32(node->val.s)));
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* id   = NULL;
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* rslt = NULL;
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* a    = NULL;
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* rslt = NULL;
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* a    = NULL;
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
    be_node* dict = NULL;
    be_node* node = NULL;
    be_node* rslt = NULL;
    be_node* list = NULL;
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
be_node* be_get_dict(be_node* node, char* key)
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
be_node* be_get_node(be_node* dict, char* key1, char* key2)
{
    be_node* node = NULL;
    vassert(dict);
    vassert(key1);
    vassert(key2);

    node = be_get_dict(dict, key1);
    retE_p((!node));
    retE_p((BE_DICT != node));

    node = be_get_dict(node, key2);
    retE_p((!node));
    return node;
}

static
int be_check_mtype(be_node* dict, int type)
{
    be_node* node = NULL;
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
    };

    if (query) && (!strcmp(node->val.s, query)) {
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
int be_get_token(be_node* dict, vtoken* token)
{
    be_node* node = NULL;
    int len = 0;
    vassert(node);
    vassert(token);

    node = be_get_dict(dict, "t");
    retE((!node));
    retE((node->type != BE_STR));

    len = INT32(UNOFF_INT32(node->val.s));
    retE((len != VTOKEN_LEN));

    memcpy(&token->data, node->val.s, len);
    return 0;
}

int be_get_id(be_node* dict, char* key1, char* key2, vnodeId* id)
{
    be_node* node = NULL;
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

    len = INT32(UNOFF_INT32(node->val.s));
    retE((VNODE_ID_LEN != len);
    memcpy(id->data, node->val.s, len);

    return 0;
}

int be_get_info(be_node* dict, vnodeInfo* info)
{
    be_node* node = NULL;
    int len = 0;
    vassert(dict);
    vassert(info);

    node = be_get_dict(dict, "id");
    retE((!node));
    retE((node->type != BE_STR));
    len = INT32(UNOFF_INT32(node->val.s));
    retE((len != VNODE_ID_LEN);
    memcpy(&info->id.data, node->val.s, len);

    node = be_get_dict(dict, "m");
    //todo;

    node = be_get_dict(dict, "v");
    retE((!node));
    retE((node->type != BE_STR);
    len = INT32(UNOFF_INT32(node->val.s));
    retE((len != VNODE_ID_LEN));
    memcpy(&info->ver.data, node->val.s, len);

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
    be_node* dict = NULL;
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
    ret = be_get_id(dict, "a", "id");
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
    be_node* dict = NULL;
    be_node* node = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(result);

    dict = be_decode(buf, sz);
    vlog_cond((!dict), elog_be_decode);
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
int _dec_find_node(void* buf, int sz, vtoken* token, vnodeId* srcId, vnodeId* targetId)
{
    be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    dict = be_decode(buf, sz);
    vlog_cond((!dict), elog_be_decode);
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
    be_node* dict = NULL;
    be_node* node = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(result);

    dict = be_decode(buf, sz);
    vlog_cond((!dict), elog_be_decode);
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
    be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    dict = be_decode(buf, sz);
    vlog_cond((!dict), elog_be_decode);
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
    be_node* dict = NULL;
    be_node* list = NULL;
    be_node* node = NULL;
    int ret = 0;
    int i = 0;

    vassert(buf);
    vassert(sz > 0);
    vassert(token);
    vassert(srcId);
    vassert(closest);

    dict = be_decode(buf, sz);
    vlog_cond((!dict), elog_be_decode);
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

static
struct vdht_dec_ops dht_dec_ops = {
    .ping                  = _dec_ping,
    .ping_rsp              = _dec_ping_rsp,
    .find_node             = _dec_find_node,
    .find_node_rsp         = _dec_find_node_rsp,
    .get_peers             = _dec_get_peers,
    .get_peers_rsp         = _dec_get_peers_rsp,
    .find_closet_nodes     = _dec_find_closest_nodes,
    .find_cloest_nodes_rsp = _dec_find_closest_nodes_rsp
};

