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

    buf = vmsg_buf_alloc(4);
    retE_p((!buf));

    //reserve msgId and magic space.
    return buf + sizeof(int32_t) + sizeof(uint32_t);
}

int vdht_buf_len(void)
{
    return (int)(4*BUF_SZ - sizeof(int32_t) - sizeof(uint32_t));
}

void vdht_buf_free(void* buf)
{
    vassert(buf);

    vmsg_buf_free(buf - sizeof(int32_t) - sizeof(uint32_t));
    return ;
}

/*
 * ping Query = {"t":"deaf",
 *               "y":"q",
 *               "q":"ping",
 *               "a":{"id":"dbfcc5576ca7f742c802"}
 *              }
 * encoded = d1:t40:deaf1:y1:q1:q4:ping1:ad2:id40:dbfcc5576ca7f742c802ee
 *
 * the method to encode DHT @ping query.
 *
 * @nonce: DHT message transaction ID.
 * @srcId: ID of source DHT node.
 * @buf:   message content.
 * @sz:    length;
 */
static
int _vdht_enc_ping(vnonce* nonce, vnodeId* srcId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* id   = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("ping");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    id   = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(node, "id", id);
    be_add_keypair(dict, "a", node);

    ret  = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * response = {"t":"06d5613f",
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
 * encoded = d1:t40:06d5613f1:y1:r1:rd4:noded2:id40:dbfcc5576ca7f742c802930892
 *           de9a1fb521f3911:v9:0.0.0.1.02:ml19:192.168.4.125:123001:wi0eeee
 *
 * the method to encode DHT reponse to @ping query.
 *
 * @nonce:
 * @srcId: ID of source DHT node.
 * @result: queried result (@nodei)
 * @buf:
 * @len:
 */
static
int _vdht_enc_ping_rsp(vnonce* nonce, vnodeId* srcId, vnodeInfo* result, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);

    node = be_create_str("ping");
    be_add_keypair(dict, "r", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(result, vnodeInfo_size(result));
    be_add_keypair(tmp, "node", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * find_node Query = {"t":"9948eb59",
 *                    "y":"q",
 *                    "q":"find_node",
 *                    "a": {"id"    :"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                          "target":"7ba29c1b9215a2e7621f13414fd21c83414eaf45"
 *                         }
 *                   }
 * bencoded = d1:t20:9948eb591:y1:q1:q9:find_node1:ad2:id20:dbfcc5576ca7f742c
 *            802930892de9a1fb521f391e6:target20:7ba29c1b9215a2e7621f13414fd21
 *            c83414eaf45eee
 *
 * the method to encode DHT @find_node query.
 *
 * @nonce:
 * @srcId:
 * @targetId: ID of DHT node to find.
 * @buf:
 * @len:
 */
static
int _vdht_enc_find_node(vnonce* nonce, vnodeId* srcId, vnodeId* targetId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("find_node");
    be_add_keypair(dict, "q", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(targetId, sizeof(vnodeId));
    be_add_keypair(tmp, "target", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 *  response = {"t":"9948eb59",
 *              "y":"r",
 *              "r":"find_node",
 *              "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                  "node" :{"id": "7ba29c1b9215a2e7621f13414fd21c83414eaf45",
 *                            "m": "192.168.4.46:12300",
 *                            "v": "0.0.0.1.0",
 *                            "f": "0"
 *                          }
 *                 }
 *            }
 * bencoded = d1:t20:9948eb591:y1:r1:rd2:id20:dbfcc5576ca7f742c802930892de9a1f
 *            b521f391:noded2:id20:7ba29c1b9215a2e7621f13414fd21c83414eaf45e1:
 *            m18:192.168.4.46:123001:v9:0.0.0.0.01:fi0eeee
 *
 * the method to encode DHT reponse to @find_node query.
 *
 * @nonce:
 * @srcId:
 * @result: nodei of DHT node being found.
 * @buf:
 * @sz:
 */
static
int _vdht_enc_find_node_rsp(vnonce* nonce, vnodeId* srcId, vnodeInfo* result, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);

    node = be_create_str("find_node");
    be_add_keypair(dict, "r", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(result, vnodeInfo_size(result));
    be_add_keypair(tmp, "node", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * find_node Query = {"t":"971ee80a",
 *                    "y":"q",
 *                    "q":"find_closest_nodes",
 *                    "a": {"id":"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                          "target":"7ba29c1b9215a2e7621f13414fd21c83414eaf45"
 *                         }
 *                   }
 * bencoded = :d1:t20:971ee80a1:y1:q1:q18:find_closest_nodes1:ad2:id20:dbfcc55
 *            76ca7f742c802930892de9a1fb521f391e6:target20:7ba29c1b9215a2e7621
 *            f13414fd21c83414eaf45eee
 *
 * the method to encode DHT @find_closest_node query.
 *
 * @nonce:
 * @srcId:
 * @targetId: ID of DHT node.
 * @buf:
 * @len:
 *
 */
static
int _vdht_enc_find_closest_nodes(
        vnonce* nonce,
        vnodeId* srcId,
        vnodeId* targetId,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(targetId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("find_closest_nodes");
    be_add_keypair(dict, "q", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(targetId, sizeof(vnodeId));
    be_add_keypair(tmp, "target", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
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
 * the method to encode DHT response to @find_closest_nodes query.
 *
 * @nonce:
 * @srcId: Id of node replying query
 * @result: queried result.
 * @buf:
 * @sz:
 *
 */
static
int _vdht_enc_find_closest_nodes_rsp(
        vnonce* nonce,
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

    vassert(nonce);
    vassert(srcId);
    vassert(closest);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);
    node = be_create_str("find_closest_nodes");
    be_add_keypair(dict, "r", node);

    rslt = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(rslt, "id", node);
    list = be_create_list();
    for (; i < varray_size(closest); i++) {
        vnodeInfo* nodei = NULL;

        nodei = (vnodeInfo*)varray_get(closest, i);
        node = be_create_buf(nodei, vnodeInfo_size(nodei));
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
 * reflect Query = {"t":"deafc137",
 *                  "y":"q",
 *                  "q":"reflex",
 *                  "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391"}
 *              }
 * encoded = d1:t40:deafc1371:y1:q1:q4:reflex1:ad2:id40:dbfcc5576ca7f742c80293
 *           0892de9a1fb521f391ee
 *
 * the method to encode DHT @reflex query.
 *
 * @nonce:
 * @srcId: ID of DHT node to get reflexive address.
 * @buf:
 * @len:
 */
static
int _vdht_enc_reflex(vnonce* nonce, vnodeId* srcId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* id   = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("reflex");
    be_add_keypair(dict, "q", node);

    node = be_create_dict();
    id   = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(node, "id", id);
    be_add_keypair(dict, "a", node);

    ret  = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 *  response = {"t":"deafc137d",
 *              "y":"r",
 *              "r":"reflex",
 *              "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                   "me" :"10.34.2.45:12300",
 *                 }
 *            }
 *
 * the method to encode DHT response to @reflex query.
 *
 * @nonce:
 * @srcId:
 * @reflexive_addr: reflexive address.
 * @buf:
 * @sz:
 */
static
int _vdht_enc_reflex_rsp(
        vnonce* nonce,
        vnodeId* srcId,
        struct vsockaddr_in* reflective_addr,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(reflective_addr);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);
    node = be_create_str("reflex");
    be_add_keypair(dict, "r", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(reflective_addr, sizeof(*reflective_addr));
    be_add_keypair(tmp,  "m", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 *
 * reflect Query = {"t":"deafc137",
 *                  "y":"q",
 *                  "q":"probe",
 *                  "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391",
 *                       "target": }
 *              }
 * the method to encode DHT @probe query.
 *
 * @nonce:
 * @srcId:
 * @destId:
 * @buf:
 * @sz:
 */
static
int _vdht_enc_probe(vnonce* nonce, vnodeId* srcId, vnodeId* destId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(destId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);
    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("probe");
    be_add_keypair(dict, "q", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(destId, sizeof(vnodeId));
    be_add_keypair(tmp, "target", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 *  response = {"t":"deafc137",
 *              "y":"r",
 *              "r":"probe",
 *              "a":{"id":"dbfcc5576ca7f742c802930892de9a1fb521f391"}
 *            }
 * the method to encode DHT response to @probe query.
 *
 * @nonce:
 * @srcId:
 * @buf:
 * @sz:
 */
static
int _vdht_enc_probe_rsp(vnonce* nonce, vnodeId* srcId, void* buf, int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);
    node = be_create_str("probe");
    be_add_keypair(dict, "r", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*

 *
 * Query = {"t":"7cf80a63",
 *          "y":"q",
 *          "q":"post_service",
 *          "a": {"id":"b790b74d9726c859c64cd54835d693b6019bcc73",
 *                "service" :{"id": "84d26f1cbea5d67d731a67c1b7ec427e40e948f9"
 *                            "m" : ["192.168.4.46:13500", "10.0.0.12:13500"]
 *                            "n" : "5"
 *                           }
 *               }
 *         }
 * encoded = d1:t40:7cf80a631:y1:q1:q12:post_service1:ad2:id40:b790b74d9726c85
 *           9c64cd54835d693b6019bcc737:serviced2:id40:84d26f1cbea5d67d731a67c
 *           1b7ec427e40e948f91:ml18:192.168.4.46:1350015:10.0.0.12:13500e1:ni
 *           5eeee
 *
 * the method to encode DHT @post_service indication.
 *
 * @nonce:
 * @srcId:
 * @srvci:
 * @buf:
 * @len:
 */
static
int _vdht_enc_post_service(
        vnonce* nonce,
        vnodeId* srcId,
        vsrvcInfo* srvci,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srvci);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);

    node = be_create_str("post_service");
    be_add_keypair(dict, "q", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(srvci, vsrvcInfo_size(srvci));
    be_add_keypair(tmp, "service", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}
/*
 * the method to encode DHT @find_service query.
 *
 * @nonce:
 * @srcId:
 * @srvci:
 * @buf:
 * @len:
 */
static
int _vdht_enc_find_service(
        vnonce* nonce,
        vnodeId* srcId,
        vsrvcHash* srvcHash,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(srvcHash);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("q");
    be_add_keypair(dict, "y", node);
    node = be_create_str("find_service");
    be_add_keypair(dict, "q", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    tmp  = be_create_buf(srvcHash, sizeof(vsrvcHash));
    be_add_keypair(tmp, "target", node);
    be_add_keypair(dict, "a", tmp);

    ret = be_encode(dict, buf, sz);
    be_free(dict);
    retE((ret < 0));
    return ret;
}

/*
 * the method to encode DHT reponse to @find_service query.
 * @nonce:
 * @srcId:
 * @result:
 * @buf:
 * @sz:
 */
static
int _vdht_enc_find_service_rsp(
        vnonce* nonce,
        vnodeId* srcId,
        vsrvcInfo* result,
        void* buf,
        int sz)
{
    struct be_node* dict = NULL;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int ret = 0;

    vassert(nonce);
    vassert(srcId);
    vassert(result);
    vassert(buf);
    vassert(sz > 0);

    dict = be_create_dict();
    retE((!dict));

    node = be_create_buf(nonce, sizeof(vnonce));
    be_add_keypair(dict, "t", node);

    node = be_create_str("r");
    be_add_keypair(dict, "y", node);
    node = be_create_str("find_service");
    be_add_keypair(dict, "r", node);

    tmp  = be_create_dict();
    node = be_create_buf(srcId, sizeof(vnodeId));
    be_add_keypair(tmp, "id", node);
    node = be_create_buf(result, vsrvcInfo_size(result));
    be_add_keypair(tmp, "service", node);
    be_add_keypair(dict, "a", tmp);

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
int _aux_unpack_dhtId(struct be_node* dict)
{
    struct be_node* node = NULL;
    vassert(dict);
    retE((BE_DICT != dict->type));

    node = be_find_node(dict, "y");
    if (!strcmp(node->val.s, "q")) {
        node = be_find_node(dict, "q");
        return vdht_get_queryId(node->val.s);
    }
    if (!strcmp(node->val.s, "r")) {
        node = be_find_node(dict, "r");
        return vdht_get_rspId(node->val.s);
    }
    return VDHT_UNKNOWN;
}

/*
 * the method to decode DHT @ping query.
 * @ctxt:
 * @nonce:
 * @srcId:
 *
 */
static
int _vdht_dec_ping(void* ctxt, vnonce* nonce, vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    node = be_find_node(dict, "a");
    node = be_find_node(node, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    return 0;
}

/*
 * the method to decode DHT reponse message to @ping query.
 * @ctxt:
 * @nonce:
 * @result:
 */
static
int _vdht_dec_ping_rsp(void* ctxt, vnonce* nonce, vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(result);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    node = be_find_node(dict, "a");
    node = be_find_node(node, "node");
    be_unpack_buf(node, result, sizeof(vnodeInfo_relax));
    return 0;
}

/*
 * the method to decode DHT "@find_node" query.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @targetId: ID of target DHT node.
 */
static
int _vdht_dec_find_node(void* ctxt, vnonce* nonce, vnodeId* srcId, vnodeId* targetId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* tmp  = NULL;
    struct be_node* node = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);
    vassert(targetId);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "target");
    be_unpack_buf(node, targetId, sizeof(vnodeId));
    return 0;
}

/*
 * the method to decode response mssage to DHT @find_node query.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @result:
 */
static
int _vdht_dec_find_node_rsp(void* ctxt, vnonce* nonce, vnodeId* srcId, vnodeInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);
    vassert(result);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "node");
    be_unpack_buf(node, result, sizeof(vnodeInfo_relax));
    return 0;
}

/*
 * the method to decode DHT @find_closest_nodes query.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @closest:
 */
static
int _vdht_dec_find_closest_nodes(void* ctxt, vnonce* nonce, vnodeId* srcId, vnodeId* targetId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* tmp  = NULL;
    struct be_node* node = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);
    vassert(targetId);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "target");
    be_unpack_buf(node, targetId, sizeof(vnodeId));
    return 0;
}

/*
 * the method to decode DHT response message to @find_closest_nodes query.
 * @ctxt:
 * @nonce:
 * @srcId: ID of DHT node sending message.
 * @closest: array of DHT nodei that close to special DHT node.
 */
static
int _vdht_dec_find_closest_nodes_rsp(
        void* ctxt,
        vnonce* nonce,
        vnodeId* srcId,
        struct varray* closest)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;
    int i = 0;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);
    vassert(closest);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    tmp  = be_find_node(tmp, "nodes");
    for (i = 0; tmp->val.l[i]; i++) {
        vnodeInfo* nodei = NULL;
        node = tmp->val.l[i];

        nodei = vnodeInfo_relax_alloc();
        if (!nodei) {
            break;
        }
        be_unpack_buf(node, nodei, sizeof(vnodeInfo_relax));
        varray_add_tail(closest, nodei);
    }
    return 0;
}

/*
 * the method to decode DHT @reflex query.
 * @ctxt:
 * @nonce:
 * @srcId:
 */
static
int _vdht_dec_reflex(void* ctxt, vnonce* nonce, vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;

    vassert(ctxt);
    vassert(nonce);
    vassert(srcId);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    node = be_find_node(dict, "a");
    node = be_find_node(node, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    return 0;
}

/*
 * the method to decode DHT reponse message to @reflex query.
 * @ctxt:
 * @nonce:
 * @srcId: ID of DHT node sending response message.
 * @reflexive_addr: reflexive address to query node.
 */
static
int _vdht_dec_reflex_rsp(
        void* ctxt,
        vnonce* nonce,
        vnodeId* srcId,
        struct vsockaddr_in* reflexive_addr)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);
    vassert(reflexive_addr);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "m");
    be_unpack_buf(node, reflexive_addr, sizeof(*reflexive_addr));
    return 0;
}

/*
 * the method to decode DHT @probe query.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @destId: ID of DHT node where @probe query was sent to.
 */
static
int _vdht_dec_probe(void* ctxt, vnonce* nonce, vnodeId* srcId, vnodeId* destId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;

    vassert(ctxt);
    vassert(nonce);
    vassert(srcId);
    vassert(destId);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "target");
    be_unpack_buf(node, destId, sizeof(vnodeId));
    return 0;
}

/*
 * the method to decode DHT response message to @probe query.
 * @ctxt:
 * @nonce:
 * @srcId:
 */
static
int _vdht_dec_probe_rsp(void* ctxt, vnonce* nonce, vnodeId* srcId)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;

    vassert(ctxt);
    vassert(nonce);
    vassert(srcId);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    node = be_find_node(dict, "a");
    node = be_find_node(node, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    return 0;
}

/*
 * the method to decode @post_service indication.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @srvci: service entry to post.
 */
static
int _vdht_dec_post_service(
        void* ctxt,
        vnonce* nonce,
        vnodeId* srcId,
        vsrvcInfo* srvci)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;

    vassert(dict);
    vassert(nonce);
    vassert(srcId);
    vassert(srvci);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "service");
    be_unpack_buf(node, srvci, sizeof(vsrvcInfo_relax));
    return 0;
}

/*
 * the method to decode DHT @find_service query.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @srvcHash:
 */
static
int _vdht_dec_find_service(
        void* ctxt,
        vnonce* nonce,
        vnodeId* srcId,
        vsrvcHash* srvcHash)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;

    vassert(ctxt);
    vassert(nonce);
    vassert(srcId);
    vassert(srvcHash);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "target");
    be_unpack_buf(node, srvcHash, sizeof(vsrvcHash));
    return 0;
}

/*
 * the method to decode DHT response to @find_service query.
 * @ctxt:
 * @nonce:
 * @srcId:
 * @result:
 */
static
int _vdht_dec_find_service_rsp(void* ctxt, vnonce* nonce, vnodeId* srcId, vsrvcInfo* result)
{
    struct be_node* dict = (struct be_node*)ctxt;
    struct be_node* node = NULL;
    struct be_node* tmp  = NULL;

    vassert(ctxt);
    vassert(nonce);
    vassert(srcId);
    vassert(result);

    node = be_find_node(dict, "t");
    be_unpack_buf(node, nonce, sizeof(vnonce));
    tmp  = be_find_node(dict, "a");
    node = be_find_node(tmp, "id");
    be_unpack_buf(node, srcId, sizeof(vnodeId));
    node = be_find_node(tmp, "service");
    be_unpack_buf(node, result, sizeof(vsrvcInfo_relax));
    return 0;
}

/*
 * the method to begine decode DHT message.
 * @buf:
 * @len:
 * @ctxt: [out].
 */
static
int _vdht_dec_begin(void* buf, int len, void** ctxt)
{
    struct be_node* dict = NULL;
    int ret = 0;

    vassert(buf);
    vassert(len);
    vassert(ctxt);

   // vlogI("[dht msg]->%s", (char*)buf);
    dict = be_decode(buf, len);
    vlogEv((!dict), elog_be_decode);
    retE((!dict));

    ret = _aux_unpack_dhtId(dict);
    ret1E((ret < 0), be_free(dict));
    ret1E((ret >= VDHT_UNKNOWN), be_free(dict));

    *ctxt = (void*)dict;
    return ret;
}

/*
 * the method of close decoder handler.
 * @ctxt:
 */
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

