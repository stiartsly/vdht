#include "vglobal.h"
#include "vdht_core.h"

static MEM_AUX_INIT(be_cache, sizeof(struct be_node), 16);
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
    return node;
}

void be_free(struct be_node* node)
{
    vassert(node);
    switch(node->type) {
    case BE_STR:
        free(unoff_addr(node->val.s, sizeof(int32_t)));
        break;
    case BE_INT:
        break;
    case BE_LIST: {
        int i = 0;

        for (; node->val.l[i]; ++i) {
            be_free(node->val.l[i]);
        }
        free(node->val.l);
        break;
    }
    case BE_DICT: {
        int i = 0;
        for (; node->val.d[i].val; ++i) {
            free(unoff_addr(node->val.d[i].key, sizeof(int32_t)));
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
int32_t _be_decode_int(char** data, int* data_len)
{
    char *endp = NULL;
    int32_t ret = 0;

    errno = 0;
    ret  = (int32_t)strtol(*data, &endp, 10);
    retE((errno));
    *data_len -= (endp - *data);
    *data = endp;
    return ret;
}

static
char* _be_decode_str(char** data, int* data_len)
{
    char* s = NULL;
    int32_t len = 0;

    len = _be_decode_int(data, data_len);
    retE_p((len < 0));
    retE_p((len > *data_len - 1));

    if (**data == ':') {
        *data += 1;
        *data_len -= 1;

        s = (char*)malloc(sizeof(len) + len + 1);
        vlog((!s), elog_malloc);
        retE_p((!s));
        memset(s, 0, sizeof(len) + len + 1);

        set_int32(s, len);
        s = offset_addr(s, sizeof(int32_t));
        strncpy(s, *data, len);

        *data += len;
        *data_len -= len;
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
        node = be_alloc(BE_STR);
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

struct be_node *be_decode(char *data, int len)
{
    return _be_decode(&data, &len);
}

struct be_node *be_decode_str(char *data)
{
    return be_decode(data, strlen(data));
}

struct be_node *be_create_dict(void)
{
    return be_decode_str("de");
}

struct be_node *be_create_list(void)
{
    return be_decode_str("le");
}

struct be_node* be_create_str(char* str)
{
    struct be_node* node = NULL;
    char* s = NULL;
    int len = strlen(str);
    vassert(str);

    node = be_alloc(BE_STR);
    vlog((!node), elog_be_alloc);
    retE_p((!node));

    s = (char*)malloc(sizeof(int32_t) + len + 1);
    vlog((!s), elog_malloc);
    ret1E_p((!s), be_free(node));

    set_int32(s, len);
    s = offset_addr(s, sizeof(int32_t));
    strcpy(s, str);

    node->val.s = s;
    return node;
}

struct be_node* be_create_int(int num)
{
    struct be_node* node = NULL;

    node = be_alloc(BE_INT);
    vlog((!node), elog_be_alloc);
    retE_p((!node));

    node->val.i = num;
    return node;
}

struct be_node* be_create_vtoken(vtoken* token)
{
    struct be_node* node = NULL;
    char buf[64];
    vassert(token);

    memset(buf, 0, 64);
    vtoken_strlize(token, buf, 64);
    node = be_create_str(buf);
    retE_p((!node));
    return node;
}

struct be_node* be_create_addr(struct sockaddr_in* addr)
{
    struct be_node* node = NULL;
    char buf[64];
    int port = 0;
    vassert(addr);

    memset(buf, 0, 64);
    vsockaddr_unconvert(addr, buf, 64, &port);
    sprintf(buf + strlen(buf), ":%d", port);

    node = be_create_str(buf);
    retE_p((!node));
    return node;
}

struct be_node* be_create_vnodeId(vnodeId* srcId)
{
    struct be_node* node = NULL;
    char buf[64];
    vassert(srcId);

    memset(buf, 0, 64);
    vnodeId_strlize(srcId, buf, 64);
    node = be_create_str(buf);
    retE_p((!node));
    return node;
}

struct be_node* be_create_ver(vnodeVer* ver)
{
    struct be_node* node = NULL;
    char buf[64];
    vassert(ver);

    memset(buf, 0, 64);
    vnodeVer_strlize(ver, buf, 64);

    node = be_create_str(buf);
    retE_p((!node));
    return node;
}

int be_add_keypair(struct be_node *dict, char *str, struct be_node *node)
{
    struct be_dict* d = NULL;
    int len = strlen(str);
    char* s = NULL;
    int i = 0;

    vassert(dict);
    vassert(str);
    vassert(dict->type == BE_DICT);

    s = (char*)malloc(sizeof(int32_t) + len + 1);
    vlog((!s), elog_malloc);
    retE((!s));

    set_int32(s, len);
    s = offset_addr(s, sizeof(int32_t));
    memcpy(s, str, len);
    s[len] = '\0';

    for (; dict->val.d[i].val; i++);
    d = (struct be_dict*)realloc(dict->val.d, (i+2)*sizeof(*d));
    vlog((!d), elog_realloc);
    ret1E((!d), free(unoff_addr(s, sizeof(int32_t))));

    dict->val.d = d;
    dict->val.d[i].key = s;
    dict->val.d[i].val = node;
    i++;
    dict->val.d[i].val = NULL;
    return 0;
}
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

int be_encode(struct be_node *node, char *buf, int len)
{
    int off = 0;
    int ret = 0;

    vassert(node);
    vassert(buf);
    vassert(len > 0);

    switch(node->type) {
    case BE_STR: {
        int _len = get_int32(unoff_addr(node->val.s, sizeof(int32_t)));
        ret = snprintf(buf+off, len-off, "%i:", _len);
        retE((ret < 0));
        off += ret;
        ret = snprintf(buf+off, len-off, "%s", node->val.s);
        retE((ret < 0));
        off += ret;
        break;
    }
    case BE_INT:
        ret = snprintf(buf+off, len-off, "i%ie", node->val.i);
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
        break;
    }
    case BE_DICT: {
        int i = 0;
        ret = snprintf(buf+off, len-off, "d");
        retE((ret < 0));
        off += ret;
        for (i = 0; node->val.d[i].val; i++) {
            char* _key = node->val.d[i].key;
            int   _len = get_int32(unoff_addr(_key, sizeof(int32_t)));
            ret = snprintf(buf + off, len - off, "%i:%s", _len, _key);
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
    if (!found) {
        return NULL;
    }
    return node->val.d[i].val;
}

int be_get_token(struct be_node* dict, vtoken* token)
{
    struct be_node* node = NULL;
    char* s = NULL;
    int len = 0;
    int ret = 0;
    vassert(dict);
    vassert(token);

    node = be_get_dict(dict, "t");
    retE((!node));
    retE((node->type != BE_STR));

    s   = unoff_addr(node->val.s, sizeof(int32_t));
    len = get_int32(s);
    retE((len != strlen(node->val.s)));

    ret = vtoken_unstrlize(node->val.s, token);
    retE((ret < 0));
    return 0;
}

