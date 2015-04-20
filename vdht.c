#include "vglobal.h"
#include "vdht.h"

struct vdhtId_desc {
    int   id;
    char* desc;
};

static
struct vdhtId_desc dhtId_global_desc[] = {
    {VDHT_PING,                 "ping"                  },
    {VDHT_PING_R,               "ping_rsp"              },
    {VDHT_FIND_NODE,            "find_node"             },
    {VDHT_FIND_NODE_R,          "find_node_rsp"         },
    {VDHT_FIND_CLOSEST_NODES,   "find_closest_nodes"    },
    {VDHT_FIND_CLOSEST_NODES_R, "find_closest_nodes_rsp"},
    {VDHT_REFLEX,               "reflex"                },
    {VDHT_REFLEX_R,             "reflex_rsp"            },
    {VDHT_PROBE,                "probe"                 },
    {VDHT_PROBE_R,              "probe_rsp"             },
    {VDHT_POST_SERVICE,         "post_service"          },
    {VDHT_FIND_SERVICE,         "find_service"          },
    {VDHT_FIND_SERVICE_R,       "find_service_rsp"      },
    {VDHT_UNKNOWN, NULL}
};

static
struct vdhtId_desc dhtId_query_desc[] = {
    {VDHT_PING,                 "ping"                  },
    {VDHT_FIND_NODE,            "find_node"             },
    {VDHT_FIND_CLOSEST_NODES,   "find_closest_nodes"    },
    {VDHT_REFLEX,               "reflex"                },
    {VDHT_PROBE,                "probe"                 },
    {VDHT_POST_SERVICE,         "post_service"          },
    {VDHT_FIND_SERVICE,         "find_service"          },
    {VDHT_UNKNOWN, NULL }
};

static
struct vdhtId_desc dhtId_rsp_desc[] = {
    {VDHT_PING_R,               "ping"                  },
    {VDHT_FIND_NODE_R,          "find_node"             },
    {VDHT_FIND_CLOSEST_NODES_R, "find_closest_nodes"    },
    {VDHT_REFLEX_R,             "reflex"                },
    {VDHT_PROBE_R,              "probe"                 },
    {VDHT_FIND_SERVICE_R,       "find_service"          },
    {VDHT_UNKNOWN, NULL }
};

char* vdht_get_desc(int dhtId)
{
    struct vdhtId_desc* desc = dhtId_global_desc;
    int i = 0;

    if ((dhtId < 0) || (dhtId >= VDHT_UNKNOWN)) {
        return "unknown dht";
    }

    for (; desc->desc; i++) {
        if (desc->id == dhtId) {
            break;
        }
        desc++;
    }
    return desc->desc;
}

int vdht_get_dhtId_by_desc(const char* dht_str)
{
    struct vdhtId_desc* desc = dhtId_global_desc;
    int dhtId = -1;

    vassert(dht_str);

    for (; desc->desc; desc++) {
        if (!strcmp(dht_str, desc->desc)) {
            dhtId = desc->id;
            break;
        }
    }
    return dhtId;
}

static
int vdht_get_queryId(char* desc)
{
    struct vdhtId_desc* id_desc = dhtId_query_desc;
    int qId = VDHT_UNKNOWN;
    vassert(desc);

    for (; id_desc->desc;) {
        if (!strcmp(id_desc->desc, desc)) {
            qId = id_desc->id;
            break;
        }
        id_desc++;
    }
    return qId;
}

static
int vdht_get_rspId(char* desc)
{
    struct vdhtId_desc* id_desc = dhtId_rsp_desc;
    int rspId = VDHT_UNKNOWN;
    vassert(desc);

    for (; id_desc->desc;) {
        if (!strcmp(id_desc->desc, desc)) {
            rspId = id_desc->id;
            break;
        }
        id_desc++;
    }
    return rspId;
}

void* vdht_buf_alloc(void)
{
    void* buf = NULL;

    buf = malloc(8*BUF_SZ);
    vlogEv((!buf), elog_malloc);
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

static
struct be_node* _aux_create_vnodeInfo(vnodeInfo* nodei)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* addr = NULL;
    int i = 0;
    vassert(nodei);

    dict = be_create_dict();
    retE_p((!dict));

    node = be_create_vtoken(&nodei->id);
    be_add_keypair(dict, "id", node);
    node = be_create_ver(&nodei->ver);
    be_add_keypair(dict, "v", node);
    node = be_create_int(nodei->weight);
    be_add_keypair(dict, "w", node);

    node = be_create_list();
    for (i = 0; i < nodei->naddrs; i++) {
        addr = be_create_addr(&nodei->addrs[i]);
        be_add_list(node, addr);
    }
    be_add_keypair(dict, "m", node);
    return dict;
}

static
struct be_node* _aux_create_vsrvcInfo(vsrvcInfo* srvci)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* addr = NULL;
    int i = 0;
    vassert(srvci);

    dict = be_create_dict();
    retE_p((!dict));

    node = be_create_vtoken(&srvci->hash);
    be_add_keypair(dict, "hash", node);
    node = be_create_vtoken(&srvci->hostid);
    be_add_keypair(dict, "id", node);
    node = be_create_int(srvci->nice);
    be_add_keypair(dict, "n", node);

    node = be_create_list();
    for (i = 0; i < srvci->naddrs; i++) {
        addr = be_create_addr(&srvci->addrs[i]);
        be_add_list(node, addr);
    }
    be_add_keypair(dict, "m", node);

    return dict;
}

/*
 * @token:
 * @srcId: Id of node sending query.
 * @buf:
 * @len:
 *
 * ping Query = {"t":"deafc137da918b8cd9b95e72fef379a5b54c3f36",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391"}
 *              }
 * encoded = d1:t40:deafc137da918b8cd9b95e72fef379a5b54c3f361:y1:q1:q4:ping1:a
 *           d2:id40:dbfcc5576ca7f742c802930892de9a1fb521f391ee
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
    id   = be_create_vtoken(srcId);
    be_add_keypair(node, "id", id);
    be_add_keypair(dict, "a", node);

    ret  = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId:  Id of node replying query.
 * @result: queried result
 * @buf:
 * @len:
 * response = {"t":"06d5613f3f43631c539db6f10fbd04b651a21844",
 *             "y":"r",
 *             "r":"ping",
 *             "a": {"id": "dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                   node" :{"id": "dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                            "v": "0.0.0.1.0",
 *                            "ml": "192.168.4.125:12300",
 *                            "w": "0"
 *                          }
 *                 }
 *            }
 * encoded = d1:t40:06d5613f3f43631c539db6f10fbd04b651a218441:y1:r1:rd4:noded
 *           2:id40:dbfcc5576ca7f742c802930892de9a1fb521f3911:v9:0.0.0.1.0
 *           2:ml19:192.168.4.125:123001:wi0eeee
 *
 */
static
int _vdht_enc_ping_rsp(vtoken* token, vnodeId* srcId, vnodeInfo* result, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* temp = NULL;
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

    node = be_create_str("ping");
    be_add_keypair(dict, "r", node);

    node = be_create_dict();
    temp = be_create_vtoken(srcId);
    be_add_keypair(node, "id", temp);
    temp = _aux_create_vnodeInfo(result);
    be_add_keypair(node, "node", temp);
    be_add_keypair(dict, "a", node);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId:  Id of node sending query.
 * @target: Id of queried node.
 * @buf:
 * @len:
 *
 * find_node Query = {"t":"9948eb5973da8f3c3c0a",
 *                    "y":"q",
 *                    "q":"find_node",
 *                    "a": {"id":"7ba29c1b9215a2e7621e",
 *                          "target":"7ba29c1b9215a2e7621"
 *                         }
 *                   }
 * bencoded =  d1:t20:9948eb5973da8f3c3c0a1:y1:q1:q9:find_node1:ad2:id20:7ba29c
 *             1b9215a2e7621e6:target20:7ba29c1b9215a2e7621eee
 */
static
int _vdht_enc_find_node(
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId,
        void* buf,
        int sz)
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
    tmp  = be_create_vtoken(srcId);
    be_add_keypair(node, "id", tmp);
    tmp  = be_create_vtoken(targetId);
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
 *  response = {"t":"9948eb5973da8f3c3c0a",
 *              "y":"r",
 *              "r":"find_node",
 *              "a":{"id":"ce3dbcf618862baf69e8",
 *                  "node" :{"id": "7ba29c1b9215a2e7621e",
 *                            "m": "192.168.4.46:12300",
 *                            "v": "0.0.0.1.0",
 *                            "f": "0"
 *                          }
 *                 }
 *            }
 * bencoded = d1:t20:9948eb5973da8f3c3c0a1:y1:r1:rd2:id20:ce3dbcf618862baf69e84
 *            :noded2:id20:7ba29c1b9215a2e7621e1:m18:192.168.4.46:123001:v9:0.0
 *            .0.0.01:fi0eeee
 */
static
int _vdht_enc_find_node_rsp(
        vtoken* token,
        vnodeId* srcId,
        vnodeInfo* result,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* temp = NULL;
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

    node = be_create_str("find_node");
    be_add_keypair(dict, "r", node);

    node = be_create_dict();
    temp = be_create_vtoken(srcId);
    be_add_keypair(node, "id", temp);
    temp = _aux_create_vnodeInfo(result);
    be_add_keypair(node, "node", temp);
    be_add_keypair(dict, "a", node);

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
 * find_node Query = {"t":"971ee80a808da2eb7fb2",
 *                    "y":"q",
 *                    "q":"find_closest_nodes",
 *                    "a": {"id":"7ba29c1b9215a2e7621e",
 *                          "target":"7ba29c1b9215a2e7621"
 *                         }
 *                   }
 * bencoded = :d1:t20:971ee80a808da2eb7fb21:y1:q1:q18:find_closest_nodes1:ad2:i
 *            d20:7ba29c1b9215a2e7621e6:target20:7ba29c1b9215a2e7621eee
 */
static
int _vdht_enc_find_closest_nodes(
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId,
        void* buf,
        int sz)
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
    tmp  = be_create_vtoken(srcId);
    be_add_keypair(node, "id", tmp);
    tmp  = be_create_vtoken(targetId);
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
 *  response = {"t":"30c6443e29cc307571e3",
 *              "y":"r",
 *              "r":"find_closest_nodes",
 *              "a":{"id":"ce3dbcf618862baf69e8",
 *                  "nodes" :["id": "5a7f5578eace25999477",
 *                            "m":  "192.168.4.46:12300",
 *                            "v":  "0.0.0.0.01",
 *                            "f": "00000001"
 *                           ],
 *                           ...
 *                 }
 *            }
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
    node = be_create_str("find_closest_nodes");
    be_add_keypair(dict, "r", node);

    rslt = be_create_dict();
    node = be_create_vtoken(srcId);
    be_add_keypair(rslt, "id", node);
    list = be_create_list();
    for (; i < varray_size(closest); i++) {
        vnodeInfo* info = NULL;

        info = (vnodeInfo*)varray_get(closest, i);
        node = _aux_create_vnodeInfo(info);
        be_add_list(list, node);
    }
    be_add_keypair(rslt, "nodes", list);
    be_add_keypair(dict, "a", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * @token:
 * @srcId: Id of node sending query.
 * @buf:
 * @len:
 * reflect Query = {"t":"deafc137da918b8cd9b95e72fef379a5b54c3f36",
 *                  "y":"q",
 *                  "q":"reflex",
 *                  "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391"}
 *              }
 * encoded = d1:t40:deafc137da918b8cd9b95e72fef379a5b54c3f361:y1:q1:q4:reflect
 *           1:ad2:id40:dbfcc5576ca7f742c802930892de9a1fb521f391ee
 */
static
int _vdht_enc_reflex(vtoken* token, vnodeId* srcId, void* buf, int sz)
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

    node = be_create_str("reflex");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    id   = be_create_vtoken(srcId);
    be_add_keypair(node, "id", id);
    be_add_keypair(dict, "a", node);

    ret  = be_encode(dict, buf, sz);
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
 *  response = {"t":"deafc137da918b8cd9b95e72fef379a5b54c3f36",
 *              "y":"r",
 *              "r":"reflex",
 *              "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                   "me" :"10.34.2.45:12300",
 *                 }
 *            }
 */
static
int _vdht_enc_reflex_rsp(
        vtoken* token,
        vnodeId* srcId,
        struct sockaddr_in* reflective_addr,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(reflective_addr);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);
    node = be_create_str("reflex");
    be_add_keypair(dict, "r", node);

    rslt = be_create_dict();
    node = be_create_vtoken(srcId);
    be_add_keypair(rslt, "id", node);
    node = be_create_addr(reflective_addr);
    be_add_keypair(rslt, "me", node);
    be_add_keypair(dict, "a", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 *
 * reflect Query = {"t":"deafc137da918b8cd9b95e72fef379a5b54c3f36",
 *                  "y":"q",
 *                  "q":"probe",
 *                  "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                       "target": }
 *              }
 */
static
int _vdht_enc_probe(
        vtoken* token,
        vnodeId* srcId,
        vnodeId* destId,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* temp = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(destId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);
    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("probe");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    temp = be_create_vtoken(srcId);
    be_add_keypair(node, "id", temp);
    temp = be_create_vtoken(destId);
    be_add_keypair(node, "target", temp);
    be_add_keypair(dict, "a", node);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 *  response = {"t":"deafc137da918b8cd9b95e72fef379a5b54c3f36",
 *              "y":"r",
 *              "r":"probe",
 *              "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391"}
 *            }
 */
static
int _vdht_enc_probe_rsp(
        vtoken* token,
        vnodeId* srcId,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* temp = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);
    node = be_create_str("probe");
    be_add_keypair(dict, "r", node);

    node = be_create_dict();
    temp = be_create_vtoken(srcId);
    be_add_keypair(node, "id", temp);
    be_add_keypair(dict, "a", node);

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
 * Query = {"t":"7cf80a63748208c08ba5b97401d52fb3baf45cbe",",
 *          "y":"q",
 *          "q":"post_service",
 *          "a": {"id":"b790b74d9726c859c64cd54835d693b6019bcc73",
 *                "service" :{"id": "84d26f1cbea5d67d731a67c1b7ec427e40e948f9"
 *                            "m" : ["192.168.4.46:13500", "10.0.0.12:13500"]
 *                            "n" : "5"
 *                           }
 *               }
 *         }
 * encoded = d1:t40:7cf80a63748208c08ba5b97401d52fb3baf45cbe1:y1:q1:q12:post_service
 *           1:ad2:id40:b790b74d9726c859c64cd54835d693b6019bcc737:serviced2:id40:
 *           84d26f1cbea5d67d731a67c1b7ec427e40e948f91:ml18:192.168.4.46:1350015:
 *           10.0.0.12:13500e1:ni5eeee
 *
 */
static
int _vdht_enc_post_service(
        vtoken* token,
        vnodeId* srcId,
        vsrvcInfo* srvci,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* rslt = NULL;
    int ret = 0;

    vassert(token);
    vassert(srvci);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("post_service");
    be_add_keypair(dict, "q", node);

    rslt = be_create_dict();
    node = be_create_vtoken(srcId);
    be_add_keypair(rslt, "id", node);

    node = _aux_create_vsrvcInfo(srvci);
    be_add_keypair(rslt, "service", node);
    be_add_keypair(dict, "a", rslt);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

static
int _vdht_enc_find_service(
        vtoken* token,
        vnodeId* srcId,
        vsrvcHash* srvcHash,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* temp = NULL;
    int ret = 0;

    vassert(token);
    vassert(srcId);
    vassert(srvcHash);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_vtoken(token);
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);
    node = be_create_str("find_service");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    temp = be_create_vtoken(srcId);
    be_add_keypair(node, "id", temp);
    temp = be_create_vtoken(srvcHash);
    be_add_keypair(node, "target", temp);
    be_add_keypair(dict, "a", node);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

static
int _vdht_enc_find_service_rsp(
        vtoken* token,
        vnodeId* srcId,
        vsrvcInfo* result,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* temp = NULL;
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
    node = be_create_str("find_service");
    be_add_keypair(dict, "r", node);

    node = be_create_dict();
    temp = be_create_vtoken(srcId);
    be_add_keypair(node, "id", temp);
    temp = _aux_create_vsrvcInfo(result);
    be_add_keypair(node, "service", temp);
    be_add_keypair(dict, "a", node);

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
    .find_closest_nodes     = _vdht_enc_find_closest_nodes,
    .find_closest_nodes_rsp = _vdht_enc_find_closest_nodes_rsp,
    .reflex                 = _vdht_enc_reflex,
    .reflex_rsp             = _vdht_enc_reflex_rsp,
    .probe                  = _vdht_enc_probe,
    .probe_rsp              = _vdht_enc_probe_rsp,
    .post_service           = _vdht_enc_post_service,
    .find_service           = _vdht_enc_find_service,
    .find_service_rsp       = _vdht_enc_find_service_rsp
};

static
int _aux_unpack_vtoken(struct be_node* dict, vtoken* token)
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
    if ((ret < 0) || (BE_STR != node->type)) {
        return -1;
    }
    ret = be_unpack_token(node, id);
    retE((ret < 0));
    return 0;
}

static
int _aux_unpack_vnodeInfo(struct be_node* dict, vnodeInfo* nodei)
{
    struct be_node* node = NULL;
    vnodeId  id;
    vnodeVer ver;
    int weight = 0;
    int i = 0;

    vassert(dict);
    vassert(nodei);
    retE((BE_DICT != dict->type));

    be_node_by_key(dict, "id", &node);
    be_unpack_token(node, &id);
    be_node_by_key(dict, "v", &node);
    be_unpack_ver(node, &ver);
    be_node_by_key(dict, "w", &node);
    be_unpack_int(node, &weight);
    be_node_by_key(dict, "m", &node);
    vassert(node->type == BE_LIST);

    vnodeInfo_relax_init((vnodeInfo_relax*)nodei, &id, &ver, weight);
    for (i = 0; node->val.l[i]; i++) {
        struct sockaddr_in addr;
        struct be_node* an = node->val.l[i];
        be_unpack_addr(an, &addr);
        vnodeInfo_add_addr(&nodei, &addr);
    }
    return 0;
}

static
int _aux_unpack_vsrvcInfo(struct be_node* dict, vsrvcInfo* srvci)
{
    struct be_node* node = NULL;
    vsrvcHash hash;
    vnodeId id;
    int nice = 0;
    int i = 0;

    vassert(dict);
    vassert(srvci);
    retE((BE_DICT != dict->type));

    be_node_by_key(dict, "hash", &node);
    be_unpack_token(node, &hash);
    be_node_by_key(dict, "id", &node);
    be_unpack_token(node, &id);
    be_node_by_key(dict, "n", &node);
    be_unpack_int(node, &nice);
    be_node_by_key(dict, "m", &node);
    vassert(node->type == BE_LIST);

    vsrvcInfo_relax_init((vsrvcInfo_relax*)srvci, &hash, &id, nice);
    for (i = 0; node->val.l[i]; i++) {
        struct sockaddr_in addr;
        struct be_node* addr_node = node->val.l[i];
        be_unpack_addr(addr_node, &addr);
        vsrvcInfo_add_addr(&srvci, &addr);
    }
    return 0;
}

static
int _aux_unpack_dhtId(struct be_node* dict)
{
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    retE((BE_DICT != dict->type));

    ret = be_node_by_key(dict, "y", &node);
    retE((ret < 0));
    retE((BE_STR != node->type));

    if (!strcmp(node->val.s, "q")) {
        ret = be_node_by_key(dict, "q", &node);
        retE((ret < 0));
        retE((BE_STR != node->type));
        return vdht_get_queryId(node->val.s);
    }
    if (!strcmp(node->val.s, "r")) {
        ret = be_node_by_key(dict, "r", &node);
        retE((ret < 0));
        retE((BE_STR != node->type));
        return vdht_get_rspId(node->val.s);
    }
    return VDHT_UNKNOWN;
}

/*
 * @ctxt:  decoder context
 * @token:
 * @srcId: source nodeId
 *
 * ping Query = {"t":"deafc137da918b8cd9b95e72fef379a5b54c3f36",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391"}
 *              }
 * encoded = d1:t40:deafc137da918b8cd9b95e72fef379a5b54c3f361:y1:q1:q4:ping1:a
 *           d2:id40:dbfcc5576ca7f742c802930892de9a1fb521f391ee
 */
static
int _vdht_dec_ping(void* ctxt, vtoken* token, vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    return 0;
}

/*
 * @ctxt:  decoder context
 * @token:
 * @result: node infos of destination node.
 *
 * response = {"t":"06d5613f3f43631c539db6f10fbd04b651a21844",
 *             "y":"r",
 *             "r":"ping",
 *             "a":{"id": "dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                  "node" :{"id": "dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                            "v": "0.0.0.1.0",
 *                            "ml": "192.168.4.125:12300",
 *                            "w": "0"
 *                          }
 *                 }
 *            }
 * encoded = d1:t40:06d5613f3f43631c539db6f10fbd04b651a218441:y1:r1:rd4:noded
 *           2:id40:dbfcc5576ca7f742c802930892de9a1fb521f3911:v9:0.0.0.1.0
 *           2:ml19:192.168.4.125:123001:wi0eeee
 */
static
int _vdht_dec_ping_rsp(
        void* ctxt,
        vtoken* token,
        vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(result);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = be_node_by_2keys(dict, "a", "node", &node);
    retE((ret < 0));
    ret = _aux_unpack_vnodeInfo(node, result);
    retE((ret < 0));

    return 0;
}

/*
 * @ctxt: decoder context
 * @token:
 * @srcId: source nodeId
 * @targetId: target node Id
 *
 * find_node Query = {"t":"9948eb5973da8f3c3c0a",
 *                    "y":"q",
 *                    "q":"find_node",
 *                    "a": {"id":"7ba29c1b9215a2e7621e",
 *                          "target":"7ba29c1b9215a2e7621"
 *                         }
 *                   }
 * bencoded =  d1:t20:9948eb5973da8f3c3c0a1:y1:q1:q9:find_node1:ad2:id20:7ba29c
 *             1b9215a2e7621e6:target20:7ba29c1b9215a2e7621eee
 */
static
int _vdht_dec_find_node(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "target", targetId);
    retE((ret < 0));

    return 0;
}

/*
 * @ctxt:
 * @token:
 * @srcId:
 * @result:
 *
  *  response = {"t":"9948eb5973da8f3c3c0a",
 *              "y":"r",
 *              "r":"find_node",
 *              "a":{"id":"ce3dbcf618862baf69e8",
 *                  "node" :{"id": "7ba29c1b9215a2e7621e",
 *                            "m": "192.168.4.46:12300",
 *                            "v": "0.0.0.1.0",
 *                            "f": "0"
 *                          }
 *                 }
 *            }
 * bencoded = d1:t20:9948eb5973da8f3c3c0a1:y1:r1:rd2:id20:ce3dbcf618862baf69e84
 *            :noded2:id20:7ba29c1b9215a2e7621e1:m18:192.168.4.46:123001:v9:0.0
 *            .0.0.01:fi0eeee
 */
static
int _vdht_dec_find_node_rsp(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(result);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = be_node_by_2keys(dict, "a", "node", &node);
    retE((ret < 0));
    ret = _aux_unpack_vnodeInfo(node, result);
    retE((ret < 0));

    return 0;
}

/*
 * @ctxt:
 * @token:
 * @srcId:
 * @closest:
 *
 * find_node Query = {"t":"971ee80a808da2eb7fb2",
 *                    "y":"q",
 *                    "q":"find_closest_nodes",
 *                    "a": {"id":"7ba29c1b9215a2e7621e",
 *                          "target":"7ba29c1b9215a2e7621"
 *                         }
 *                   }
 * bencoded = :d1:t20:971ee80a808da2eb7fb21:y1:q1:q18:find_closest_nodes1:ad2:i
 *            d20:7ba29c1b9215a2e7621e6:target20:7ba29c1b9215a2e7621eee
 */
static
int _vdht_dec_find_closest_nodes(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vnodeId* targetId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(targetId);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "target", targetId);
    retE((ret < 0));

    return 0;
}

/*
 * @ctxt:  decoder context
 * @token: msg token;
 * @srcId: source vnodeId
 * @closest: array of nodes that close to current node.
 *
 * response = {"t":"30c6443e29cc307571e3",
 *              "y":"r",
 *              "r":"find_closest_nodes",
 *              "a":{"id":"ce3dbcf618862baf69e8",
 *                  "nodes" :["id": "5a7f5578eace25999477",
 *                            "m":  "192.168.4.46:12300",
 *                            "v":  "0.0.0.0.01",
 *                            "f": "00000001"
 *                           ],
 *                           ...
 *                 }
 *            }
 * bencoded = d1:t20:30c6443e29cc307571e31:y1:r1:rd2:id20:ce3dbcf618862baf69e8\
 *            5:nodesld2:id20:5a7f5578eace259994771:m18:192.168.4.46:123001:v\
 *            9:0.0.0.0.01:fi1417421748eeeee
 */
static
int _vdht_dec_find_closest_nodes_rsp(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        struct varray* closest)
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

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = be_node_by_2keys(dict, "a", "nodes", &list);
    retE((ret < 0));
    retE((BE_LIST != list->type));

    for (i = 0; list->val.l[i]; i++) {
        vnodeInfo* nodei = NULL;
        node = list->val.l[i];
        retE((BE_DICT != node->type));

        nodei = (vnodeInfo*)vnodeInfo_relax_alloc();
        if (!nodei) {
            break;
        }
        _aux_unpack_vnodeInfo(node, nodei);
        varray_add_tail(closest, nodei);
    }

    return 0;
}

/*
 * the routine to decode @reflex query.
 * @ctxt:  deocde context.
 * @token: transaction token
 * @srcId: id of query node.
 * @addr : local address to get it's reflexive address.
 */
static
int _vdht_dec_reflex(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(srcId);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to decode response msg to @reflex query.
 * @ctxt:  deocde context.
 * @token: transaction token
 * @srcId: id of node where response came from.
 * @reflexive_addr: reflexive address to query node.
 */
static
int _vdht_dec_reflex_rsp(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        struct sockaddr_in* reflexive_addr)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(reflexive_addr);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = be_node_by_2keys(dict, "a", "me", &node);
    retE((ret < 0));
    ret = be_unpack_addr(node, reflexive_addr);
    retE((ret < 0));

    return 0;
}

/*
 * the routine to decode @probe query.
 * @ctxt:  deocde context.
 * @token: transaction token
 * @srcId: id of query node.
 * @destId: id of destination node.
 */
static
int _vdht_dec_probe(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vnodeId* destId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(srcId);
    vassert(destId);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "target", destId);
    retE((ret < 0));

    return 0;
}

/*
 * the routine to decode response msg to @probe query.
 * @ctxt:  deocde context.
 * @token: transaction token
 * @srcId: id of node where response came from.
 */
static
int _vdht_dec_probe_rsp(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(srcId);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    return 0;
}

/* the routine to decode @post_service indication.
 * @ctxt: decode context
 * @token: trans token
 * @srcId: Id of source node where the indcation was from.
 * @service:
 *
 * Query = {"t":"b6e0855abbf93b8e6754",",
 *          "y":"q",
 *          "q":"post_service",
 *          "a": {"id":"7ba29c1b9215a2e7621e",
 *                "service" :{"id": "e532d7c80cf02e8652d3"
 *                            "m" : ["192.168.4.46:14444",
 *                                   "27.115.62.114:15300"
 *                                  ]
 *                            "f" : "0"
 *                           }
 *               }
 *         }
 * bencoded = d1:t20:b6e0855abbf93b8e67541:y1:q1:q12:post_service1:ad2:id20:
 *            7ba29c1b9215a2e7621e7:serviced2:id20:e532d7c80cf02e8652d31:m17:
 *            192.168.4.46:144441:fi0eeee
 */
static
int _vdht_dec_post_service(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vsrvcInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(dict);
    vassert(token);
    vassert(srcId);
    vassert(result);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = be_node_by_2keys(dict, "a", "service", &node);
    retE((ret < 0));
    ret = _aux_unpack_vsrvcInfo(node, result);
    retE((ret < 0));

    return 0;
}

/*
 * the routine to decode @find_service query.
 * @ctxt:
 * @token:
 * @srcId:
 * @srvcId:
 */
static
int _vdht_dec_find_service(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vsrvcHash* srvcHash)
{
    struct be_node* dict = (struct be_node*)ctxt;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(srcId);
    vassert(srvcHash);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = _aux_unpack_vnodeId(dict, "a", "target", srvcHash);
    retE((ret < 0));

    return 0;
}

/*
 * the routine to decode response msg to @find_servcie query.
 * @ctxt:
 * @token:
 * @srcId:
 * @result:
 */
static
int _vdht_dec_find_service_rsp(
        void* ctxt,
        vtoken* token,
        vnodeId* srcId,
        vsrvcInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    int ret = 0;

    vassert(ctxt);
    vassert(token);
    vassert(srcId);
    vassert(result);

    ret = _aux_unpack_vtoken(dict, token);
    retE((ret < 0));

    ret = _aux_unpack_vnodeId(dict, "a", "id", srcId);
    retE((ret < 0));
    ret = be_node_by_2keys(dict, "a", "service", &node);
    retE((ret < 0));
    ret = _aux_unpack_vsrvcInfo(node, result);
    retE((ret < 0));

    return 0;
}

static
int _vdht_dec_begin(void* buf, int len, void** ctxt)
{
    struct be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(len);
    vassert(ctxt);

    vlogI("[dht msg]->%s", (char*)buf);
    dict = be_decode(buf, len);
    vlogEv((!dict), elog_be_decode);
    retE((!dict));

    ret = _aux_unpack_dhtId(dict);
    ret1E((ret < 0), be_free(dict));
    ret1E((ret >= VDHT_UNKNOWN), be_free(dict));

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
    .dec_begin              = _vdht_dec_begin,
    .dec_done               = _vdht_dec_done,

    .ping                   = _vdht_dec_ping,
    .ping_rsp               = _vdht_dec_ping_rsp,
    .find_node              = _vdht_dec_find_node,
    .find_node_rsp          = _vdht_dec_find_node_rsp,
    .find_closest_nodes     = _vdht_dec_find_closest_nodes,
    .find_closest_nodes_rsp = _vdht_dec_find_closest_nodes_rsp,
    .reflex                 = _vdht_dec_reflex,
    .reflex_rsp             = _vdht_dec_reflex_rsp,
    .probe                  = _vdht_dec_probe,
    .probe_rsp              = _vdht_dec_probe_rsp,
    .post_service           = _vdht_dec_post_service,
    .find_service           = _vdht_dec_find_service,
    .find_service_rsp       = _vdht_dec_find_service_rsp
};

