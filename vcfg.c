#include "vglobal.h"
#include "vcfg.h"

static char* def_cfg_key = "__dht_def_empty_key__";

static MEM_AUX_INIT(cfg_item_cache, sizeof(struct vcfg_item), 8);
static
struct vcfg_item* vcfg_item_alloc(int type)
{
    struct vcfg_item* item = NULL;

    item = (struct vcfg_item*)vmem_aux_alloc(&cfg_item_cache);
    vlogEv((!item), elog_vmem_aux_alloc);
    retE_p((!item));

    memset(item, 0, sizeof(*item));
    item->type  = type;
    item->depth = 0;
    return item;
}

static
void vcfg_item_init(struct vcfg_item* item, int depth)
{
    vassert(item);

    item->depth = depth;
    switch(item->type) {
    case CFG_DICT:
        vdict_init(&item->val.d);
        break;
    case CFG_LIST:
        item->val.l.key = NULL;
        varray_init(&item->val.l.l, 4);
        break;
    case CFG_TUPLE:
        varray_init(&item->val.t, 4);
        break;
    case CFG_STR:
        item->val.s = NULL;
        break;
    default:
        vassert(0);
        break;
    }
    return ;
}

static
void _aux_cfg_item_free(void* item, void* cookie)
{
    extern void vcfg_item_free(struct vcfg_item*);

    vcfg_item_free((struct vcfg_item*)item);
    return ;
}

void vcfg_item_free(struct vcfg_item* item)
{
    vassert(item);

    switch(item->type) {
    case CFG_DICT:
        vdict_zero(&item->val.d, _aux_cfg_item_free, NULL);
        vdict_deinit(&item->val.d);
        break;
    case CFG_LIST:
        if (item->val.l.key) {
            free(item->val.l.key);
        }
        varray_zero(&item->val.l.l, _aux_cfg_item_free, NULL);
        varray_deinit(&item->val.l.l);
        break;
    case CFG_TUPLE:
        varray_zero(&item->val.t, _aux_cfg_item_free, NULL);
        varray_deinit(&item->val.t);
        break;
    case CFG_STR:
        free((void*)item->val.s);
        break;
    default:
        vassert(0);
        break;
    }
    vmem_aux_free(&cfg_item_cache, item);
    return ;
}

extern void vcfg_item_dump(struct vcfg_item*);
static
void _aux_dump_align(int depth)
{
    int i = 0;
    vassert(depth >= 0);

    printf("##");
    for (i = 1; i < depth; i++) {
        printf("  ");
    }
    return ;
}

static
int _aux_dump_dict_item(char* key, void* val, void* cookie)
{
    struct vcfg_item* item = (struct vcfg_item*)val;
    vassert(key);
    vassert(val);

    _aux_dump_align(item->depth);
    if (strcmp(key, def_cfg_key)) {
        printf("%s: ", key);
    }
    vcfg_item_dump(item);
    return 0;
}

static
int _aux_dump_list_item(void* val, void* cookie)
{
    struct vcfg_item* prev = (struct vcfg_item*)cookie;
    struct vcfg_item* item = (struct vcfg_item*)val;
    vassert(item);

    switch(prev->type) {
    case CFG_LIST:
        if (prev->val.l.key) {
            _aux_dump_align(item->depth);
            printf("%s: ", prev->val.l.key);
        }
        break;
    case CFG_TUPLE:
        _aux_dump_align(item->depth);
        break;
    default:
        vassert(0);
        break;
    }
    vcfg_item_dump(item);
    return 0;
}

void vcfg_item_dump(struct vcfg_item* item)
{
    vassert(item);

    switch(item->type) {
    case CFG_DICT:
        if (item->depth > 0) {
            printf("{\n");
        }
        vdict_iterate(&item->val.d, _aux_dump_dict_item, NULL);
        _aux_dump_align(item->depth);
        if (item->depth > 0) {
            printf("}\n");
        }
        break;
    case CFG_LIST:
        printf("[\n");
        varray_iterate(&item->val.l.l, _aux_dump_list_item, item);
        _aux_dump_align(item->depth);
        printf("]\n");
        break;
    case CFG_TUPLE:
        printf("(\n");
        varray_iterate(&item->val.t, _aux_dump_list_item, item);
        _aux_dump_align(item->depth);
        printf(")\n");
        break;
    case CFG_STR:
        printf("%s\n", item->val.s);
        break;
    default:
        vassert(0);
        break;
    }
    return ;
}

static
struct vcfg_item* vcfg_item_get(struct vcfg_item* item, char* key)
{
    struct vcfg_item* tgt = NULL;
    vassert(item);
    vassert(key);

    switch(item->type) {
    case CFG_DICT:
        tgt = (struct vcfg_item*)vdict_get(&item->val.d, key);
        break;
    case CFG_LIST:
        if (!strcmp(key, item->val.l.key)) {
            tgt = item;
        }
        break;
    case CFG_TUPLE: {
        struct vcfg_item* item = NULL;
        int i = 0;
        for (i = 0; i < varray_size(&item->val.t); i++) {
            item = (struct vcfg_item*)varray_get(&item->val.t, i);
            tgt = vcfg_item_get(item, key);
            if (tgt) {
                break;
            }
        }
        break;
    }
    case CFG_STR:
        break;
    default:
        vassert(0);
        break;
    }
    return tgt;
}

static
void _aux_eat_newline(char** cur)
{
    char seps[] = " \t\n";
    vassert(cur && *cur);

    while (strchr(seps, **cur)) {
        (*cur)++;
    }
    return;
}

static
void _aux_eat_blanks(char** cur)
{
    char seps[] = " \t";
    vassert(cur && *cur);

    while (strchr(seps, **cur)) {
        (*cur)++;
    }
    return;
}

static
void _aux_eat_endings(char** cur)
{
    char seps[] = " \t\n";
    vassert(cur && *cur);

    while ((**cur != '\0') && strchr(seps, **cur)) {
        (*cur)++;
    }
    return;
}

static
void _aux_eat_comments(char** cur)
{
    char seps[] = "\n";
    vassert(cur && *cur);

    while (!strchr(seps, **cur)) {
        (*cur)++;
    }
    return;
}

static
int _aux_eat_separator(char** cur)
{
    char seps1[] = "abcdefghijklmnopqrstuvwxyz";
    char seps2[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char seps3[] = "0123456789";
    char seps4[] = "({[/";

    vassert(cur && *cur);
    while (!strchr(seps1, **cur) &&
           !strchr(seps2, **cur) &&
           !strchr(seps3, **cur) &&
           !strchr(seps4, **cur)) {
        switch(**cur) {
        case ' ':
        case '\t':
            (*cur)++;
            break;
        default:
            retE(1);
            break;
        }
    }
    return 0;
}

static
char* _aux_create_key(char** cur)
{
    char seps[] = ";:\n{}[]()";
    char* key = NULL;
    int blank = 0;
    int sz = 0;

    vassert(cur && *cur);

    key = (char*)malloc(32);
    vlogEv((!key), elog_malloc);
    memset(key, 0, 32);
    retE_p((!key));

    while (!strchr(seps, **cur)) {
        switch(**cur) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            if (key[0] && blank) {
                key[sz++] = '_';
                blank = 0;
            }
            key[sz++] = **cur;
            break;
        case ' ':
        case '\t':
            if (key[0] && !blank) {
                blank = 1;
            }
            break;
        default:
            ret1E_p((1), free(key));
            break;
        }
        (*cur)++;
        ret1E_p((sz >= 30), free(key));
    }
    key[sz] = '\0';
    return key;
}

static
char* _aux_create_val(char** cur)
{
    char seps[] = ";:\n}])";
    char* pos = *cur;
    char* val = NULL;
    int blank = 0;
    int sz = 0;

    vassert(cur && *cur);

    while(!strchr(seps, *pos)) {
        switch(*pos) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
        case '/':
        case '.':
        case '_':
            sz++;
            break;
        case ' ':
        case '\t':
            if ((sz > 0) && !blank) {
                blank = 1;
            }
            break;
        default:
            retE_p((1));
            break;
        }
        pos++;
    }

    val = (char*)malloc(sz + 1);
    vlogEv((!val), elog_malloc);
    retE_p((!val));
    memset(val, 0, sz + 1);

    blank = 0;
    sz = 0;
    while(!strchr(seps, **cur)) {
        switch(**cur) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
        case '/':
        case '.':
        case '_':
            if (blank) {
                val[sz++] = '_';
                blank = 0;
            }
            val[sz++] = **cur;
            break;
        case ' ':
        case '\t':
            if (val[0] && !blank) {
                blank = 1;
            }
            break;
        default:
            ret1E_p((1), free(val));
            break;
        }
        (*cur)++;
    }
    val[sz] = '\0';
    return val;
}

static
int _aux_parse_str(struct vcfg_item* str, char** cur)
{
    char* val = NULL;

    vassert(str);
    vassert(str->type == CFG_STR);
    vassert(cur && *cur);

    val = _aux_create_val(cur);
    retE((!val));
    str->val.s = val;

    return 0;
}

static
int _aux_parse_tuple(struct vcfg_item* tuple, char** cur)
{
    char seps[] = "\0";
    struct vcfg_item* item = NULL;
    int ret = 0;

    vassert(tuple);
    vassert(tuple->type == CFG_TUPLE);
    vassert(cur && *cur);

    while (!strchr(seps, **cur)) {
        switch(**cur) {
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            item = vcfg_item_alloc(CFG_STR);
            retE((!item));
            vcfg_item_init(item, tuple->depth + 1);

            ret = _aux_parse_str(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            varray_add_tail(&tuple->val.t, item);
            break;
        case ')':
            (*cur)++;
            _aux_eat_endings(cur);
            return 0;
            break;
        case ' ':
        case '\t':
            (*cur)++;
            _aux_eat_blanks(cur);
            break;
        case '\n':
            (*cur)++;
            _aux_eat_newline(cur);
            break;
        default:
            retE((1));
            break;
        }
    }
    return 0;
}

int _aux_parse_list(struct vcfg_item* list, char** cur)
{
    extern int _aux_parse_dict(struct vcfg_item*, char**);

    char seps[] = "\0";
    struct vcfg_item* item = NULL;
    char* key = NULL;
    int ret = 0;

    vassert(list);
    vassert(list->type == CFG_LIST);
    vassert(cur && *cur);

    while(!strchr(seps, **cur)) {
        switch(**cur) {
        case ';':
            (*cur)++;
            _aux_eat_comments(cur);
            break;
        case ':':
            (*cur)++;
            ret = _aux_eat_separator(cur);
            retE((ret < 0));
            break;
        case '{':
            (*cur)++;
            item = vcfg_item_alloc(CFG_DICT);
            retE((!item));
            vcfg_item_init(item, list->depth + 1);

            ret = _aux_parse_dict(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            varray_add_tail(&list->val.l.l, item);
            break;
        case '[':
            (*cur)++;
            item = vcfg_item_alloc(CFG_LIST);
            retE((!item));
            vcfg_item_init(item, list->depth + 1);
            ret  = _aux_parse_list(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            varray_add_tail(&list->val.l.l, item);
            break;
        case '(':
            (*cur)++;
            item = vcfg_item_alloc(CFG_TUPLE);
            retE((!item));
            vcfg_item_init(item, list->depth + 1);
            ret = _aux_parse_tuple(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            varray_add_tail(&list->val.l.l, item);
            break;
        case ']':
            (*cur)++;
            _aux_eat_endings(cur);
            return 0;
            break;
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            key = _aux_create_key(cur);
            retE((!key));
            if (!list->val.l.key) {
                list->val.l.key = key;
            }else {
                ret1E((strcmp(key, list->val.l.key)), free(key));
            }
            break;
        case ' ':
        case '\t':
            (*cur)++;
            _aux_eat_blanks(cur);
            break;
        case '\n':
            (*cur)++;
            _aux_eat_newline(cur);
            break;
        default:
            retE((1));
            break;
        }
    }
    return 0;
}

int _aux_parse_dict(struct vcfg_item* dict, char** cur)
{
    char seps[] = "\0";
    struct vcfg_item* item = NULL;
    char* key = NULL;
    int ret = 0;

    vassert(dict);
    vassert(dict->type == CFG_DICT);
    vassert(cur && *cur);

    while(!strchr(seps, **cur)) {
        switch(**cur) {
        case ';':
            (*cur)++;
            _aux_eat_comments(cur);
            break;
        case ':':
            (*cur)++;
            ret = _aux_eat_separator(cur);
            retE((ret < 0));
            retE((!key));
            break;
        case '{':
            (*cur)++;
            item = vcfg_item_alloc(CFG_DICT);
            retE((!item));
            vcfg_item_init(item, dict->depth + 1);

            ret = _aux_parse_dict(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            retE((!key));
            vdict_add(&dict->val.d, key, item);
            free(key);
            key = NULL;
            break;
        case '[':
            (*cur)++;
            item = vcfg_item_alloc(CFG_LIST);
            retE((!item));
            vcfg_item_init(item, dict->depth + 1);

            ret = _aux_parse_list(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            retE((!key));
            vdict_add(&dict->val.d, key, item);
            free(key);
            key  = NULL;
            break;
        case '(':
            (*cur)++;
            item = vcfg_item_alloc(CFG_TUPLE);
            retE((!item));
            vcfg_item_init(item, dict->depth + 1);
            ret = _aux_parse_tuple(item, cur);
            ret1E((ret < 0), vcfg_item_free(item));

            retE((!key));
            vdict_add(&dict->val.d, key, item);
            free(key);
            key = NULL;
            break;
        case '}':
            (*cur)++;
            _aux_eat_endings(cur);
            return 0;
            break;
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
        case '/':
            if (key) {
                item = vcfg_item_alloc(CFG_STR);
                retE((!item));
                vcfg_item_init(item, dict->depth + 1);

                ret = _aux_parse_str(item, cur);
                ret1E((ret < 0), vcfg_item_free(item));

                vdict_add(&dict->val.d, key, item);
                free(key);
                key = NULL;
            } else {
                key = _aux_create_key(cur);
                retE((!key));
            }
            break;
        case ' ':
        case '\t':
            (*cur)++;
            _aux_eat_blanks(cur);
            break;
        case '\n':
            (*cur)++;
            _aux_eat_newline(cur);
            break;
        default:
            retE((1));
            break;
        }
    }
    return 0;
}

/*
 * the routine to load all config items by parsing config file.
 * @cfg:
 * @filename
 */
static
int _vcfg_parse(struct vconfig* cfg, const char* filename)
{
    struct stat stat;
    char* buf = NULL;
    char* cur = NULL;
    int fd  = 0;
    int ret = 0;

    vassert(cfg);
    vassert(filename);

    fd = open(filename, O_RDONLY);
    vlogEv((ret < 0), elog_open);
    retE((fd < 0));

    memset(&stat, 0, sizeof(stat));
    ret = fstat(fd, &stat);
    vlogEv((ret < 0),elog_fstat);
    ret1E((ret < 0), close(fd));

    buf = malloc(stat.st_size + 1);
    vlogEv((!buf), elog_malloc);
    ret1E((!buf), close(fd));

    ret = read(fd, buf, stat.st_size);
    close(fd);
    vlogEv((!buf), elog_read);
    ret1E((!buf), free(buf));

    buf[stat.st_size] = '\0';

    cur = buf;
    ret = _aux_parse_dict(&cfg->dict, (char**)&cur);
    free(buf);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to clear all config items
 * @cfg:
 */
static
int _vcfg_clear(struct vconfig* cfg)
{
    vassert(cfg);
    vcfg_item_free(&cfg->dict);
    return 0;
}

/*
 * the routine to dump all config items.
 * @cfg:
 */
static
void _vcfg_dump(struct vconfig* cfg)
{
    vassert(cfg);
    vdump(printf("-> CFG"));
    vcfg_item_dump(&cfg->dict);
    vdump(printf("<- CFG"));
    return ;
}

static
struct vcfg_item* _aux_get_cfg_item(struct vcfg_item* item, const char* long_key)
{
    struct vcfg_item* sub_item = NULL;
    char* comma = NULL;
    char* key   = NULL;

    vassert(item);
    vassert(long_key);

    comma = strchr(long_key, '.');
    if (!comma) {
        return vcfg_item_get(item, (char*)long_key);
    } else {
        key = strndup(long_key, comma-long_key);
    }

    sub_item = vcfg_item_get(item, key);
    free(key);
    retE_p((!sub_item));

    return _aux_get_cfg_item(sub_item, ++comma);
}

static
int _vcfg_check(struct vconfig* cfg, const char* key)
{
    struct vcfg_item* item = NULL;
    vassert(cfg);
    vassert(key);

    item = _aux_get_cfg_item(&cfg->dict, (char*)key);
    return (!!item);
}

static
int _vcfg_get_int_val(struct vconfig* cfg, const char* key)
{
    struct vcfg_item* item = NULL;
    char* cur = NULL;
    int ret = 0;

    vassert(cfg);
    vassert(key);

    item = _aux_get_cfg_item(&cfg->dict, (char*)key);
    retE((!item));
    retE((CFG_STR != item->type));

    // check if numeric number
    cur = item->val.s;
    while (*cur != '\0') {
        if (!isdigit(*cur)) {
            break;
        } else {
            cur++;
        }
    }
    retE((*cur != '\0'));

    errno = 0;
    ret = strtol(item->val.s, NULL, 10);
    retE((errno));
    return ret;
}

static
const char* _vcfg_get_str_val(struct vconfig* cfg, const char* key)
{
    struct vcfg_item* item  = NULL;
    vassert(cfg);
    vassert(key);

    item = _aux_get_cfg_item(&cfg->dict, (char*)key);
    retE_p((!item));
    retE_p((CFG_STR != item->type));
    return item->val.s;
}

static
struct vdict* _vcfg_get_dict_val(struct vconfig* cfg, const char* key)
{
    struct vcfg_item* item = NULL;
    vassert(cfg);
    vassert(key);

    item = _aux_get_cfg_item(&cfg->dict, (char*)key);
    retE_p((!item));
    retE_p((CFG_DICT != item->type));
    return &item->val.d;
}

static
struct varray* _vcfg_get_list_val(struct vconfig* cfg, const char* key)
{
    struct vcfg_item* item = NULL;
    vassert(cfg);
    vassert(key);

    item = _aux_get_cfg_item(&cfg->dict, (char*)key);
    vlogIv((!item), "no boot nodes in config file");
    if (!item) {
        return NULL;
    }
    retE_p((CFG_LIST != item->type));
    return &item->val.l.l;
}

static
struct varray* _vcfg_get_tuple_val(struct vconfig* cfg, const char* key)
{
    struct vcfg_item* item = NULL;
    vassert(cfg);
    vassert(key);

    item = _aux_get_cfg_item(&cfg->dict, (char*)key);
    retE_p((!item));
    retE_p((CFG_TUPLE != item->type));
    return &item->val.t;
}

struct vconfig_ops cfg_ops = {
    .parse         = _vcfg_parse,
    .clear         = _vcfg_clear,
    .dump          = _vcfg_dump,
    .check         = _vcfg_check,
    .get_int_val   = _vcfg_get_int_val,
    .get_str_val   = _vcfg_get_str_val,
    .get_dict_val  = _vcfg_get_dict_val,
    .get_list_val  = _vcfg_get_list_val,
    .get_tuple_val = _vcfg_get_tuple_val
};


static
int _vcfg_get_syslog_switch(struct vconfig* cfg)
{
    int on = 1;
    vassert(cfg);

    on = cfg->ops->get_int_val(cfg, "global.syslog");
    if (on < 0) {
        on = 1;
    }
    return on;
}

static
const char* _vcfg_get_syslog_ident(struct vconfig* cfg)
{
    const char* ident = NULL;
    vassert(cfg);

    ident = cfg->ops->get_str_val(cfg, "global.syslog_ident");
    if (!ident) {
        ident = "vdhtd";
    }
    return ident;
}

static
const char* _vcfg_get_lsctl_unix_path(struct vconfig* cfg)
{
    const char* unix_path = NULL;
    vassert(cfg);

    unix_path = cfg->ops->get_str_val(cfg, "lsctl.unix_path");
    if (!unix_path) {
        unix_path = "/var/run/vdht/lsctl_socket";
    }
    return unix_path;
}

static
int _aux_tuple_addr_2_sin_addr(struct varray* tuple_addr, struct sockaddr_in* sin_addr)
{
    struct vcfg_item* item = NULL;
    char ip[64] = {'\0'};
    char* port_str = NULL;
    char* proto_str = NULL;
    int ret = 0;

    vassert(tuple_addr);
    vassert(sin_addr);

    item = (struct vcfg_item*)varray_get(tuple_addr, 0);
    retE((!item));
    retE((CFG_STR != item->type));
    retE((strlen(item->val.s) >= 64));
    strcpy(ip, item->val.s);

    item = (struct vcfg_item*)varray_get(tuple_addr, 1);
    retE((!item));
    retE((CFG_STR != item->type));
    port_str = item->val.s;

    item = (struct vcfg_item*)varray_get(tuple_addr, 2);
    retE((!item));
    retE((CFG_STR != item->type));
    proto_str = item->val.s;

    ret = vsockaddr_get_by_hostname(ip, port_str, proto_str, sin_addr);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_load_boot_nodes(struct vconfig* cfg, vcfg_load_boot_node_t cb, void* cookie)
{
    struct varray* boot_nodes = NULL;
    int ret = 0;
    int i = 0;

    vassert(cfg);
    vassert(cb);

    boot_nodes = cfg->ops->get_list_val(cfg, "boot");
    retS((!boot_nodes));

    for (i = 0; i < varray_size(boot_nodes); i++) {
        struct varray* tuple_addr = NULL;
        struct sockaddr_in sin_addr;
        tuple_addr = &((struct vcfg_item*)varray_get(boot_nodes, i))->val.t;
        ret = _aux_tuple_addr_2_sin_addr(tuple_addr, &sin_addr);
        if (ret < 0) {
            continue;
        }
        if (cb(&sin_addr, cookie) < 0) {
            continue;
        }
    }
    return 0;
}

static
int _vcfg_get_host_tick_tmo(struct vconfig* cfg)
{
    const char* val = NULL;
    int tms = 0;
    int ret = 0;

    vassert(cfg);
    val = cfg->ops->get_str_val(cfg, "global.tick_timeout");
    if (!val) {
        goto error_exit;
    }

    ret = strlen(val);
    switch(val[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        goto error_exit;
        break;
    }
    errno = 0;
    ret = strtol(val, NULL, 10);
    if (errno) {
        goto error_exit;
    }
    return (ret* tms);

error_exit:
    return (5);
}

static
int _vcfg_get_route_db_file(struct vconfig* cfg, char* db, int len)
{
    const char* file = NULL;

    vassert(cfg);
    vassert(db);
    vassert(len > 0);

    file = cfg->ops->get_str_val(cfg, "route.db_file");
    if (!file) {
        file = "route.db";
    }
    retE((strlen(file) + 1 > len));
    strcpy(db, file);
    return 0;
}

static
int _vcfg_get_route_bucket_sz(struct vconfig* cfg)
{
    int sz = 0;
    vassert(cfg);

    sz = cfg->ops->get_int_val(cfg, "route.bucket_size");
    if (sz < 0) {
        sz = 10;
    }
    return sz;
}

static
int _vcfg_get_route_max_snd_tms(struct vconfig* cfg)
{
    int tms = 0;
    vassert(cfg);

    tms = cfg->ops->get_int_val(cfg, "route.max_send_times");
    if (tms < 0) {
        tms = 5;
    }
    return tms;
}

static
int _vcfg_get_route_max_rcv_tmo(struct vconfig* cfg)
{
    const char* val = NULL;
    int tms = 0;
    int ret = 0;

    vassert(cfg);

    val = cfg->ops->get_str_val(cfg, "global.tick_timeout");
    if (!val) {
        goto error_exit;
    }

    ret = strlen(val);
    switch(val[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        goto error_exit;
        break;
    }
    errno = 0;
    ret = strtol(val, NULL, 10);
    if (errno) {
        goto error_exit;
    }
    return (ret * tms);

error_exit:
    return (60); //default value;
}

static
int _aux_get_addr_port(struct vconfig* cfg, const char* key, int* port)
{
    struct varray* tuple = NULL;
    struct vcfg_item* item = NULL;

    vassert(cfg);
    vassert(key);
    vassert(port);

    tuple = cfg->ops->get_tuple_val(cfg, key);
    retE((!tuple));

    item = (struct vcfg_item*)varray_get(tuple, 1);
    retE((!item));
    retE((item->type != CFG_STR));
    errno = 0;
    *port = strtol(item->val.s, NULL, 10);
    retE((errno));

    return 0;
}

static
int _vcfg_get_dht_port(struct vconfig* cfg)
{
    int port = 0;
    int ret = 0;
    vassert(cfg);

    ret = _aux_get_addr_port(cfg, "dht.address", &port);
    if (ret < 0) {
        port = (12300);
    }
    return port;
}

static
struct vconfig_ext_ops cfg_ext_ops = {
    .get_syslog_switch      = _vcfg_get_syslog_switch,
    .get_syslog_ident       = _vcfg_get_syslog_ident,

    .get_lsctl_unix_path    = _vcfg_get_lsctl_unix_path,
    .get_boot_nodes         = _vcfg_load_boot_nodes,
    .get_host_tick_tmo      = _vcfg_get_host_tick_tmo,

    .get_route_db_file      = _vcfg_get_route_db_file,
    .get_route_bucket_sz    = _vcfg_get_route_bucket_sz,
    .get_route_max_snd_tms  = _vcfg_get_route_max_snd_tms,
    .get_route_max_rcv_tmo  = _vcfg_get_route_max_rcv_tmo,
    .get_dht_port           = _vcfg_get_dht_port
};

int vconfig_init(struct vconfig* cfg)
{
    vassert(cfg);

    cfg->dict.type = CFG_DICT;
    vcfg_item_init(&cfg->dict, 0);
    cfg->ops     = &cfg_ops;
    cfg->ext_ops = &cfg_ext_ops;
    return 0;
}

void vconfig_deinit(struct vconfig* cfg)
{
    vassert(cfg);

    cfg->ops->clear(cfg);
    vdict_deinit(&cfg->dict.val.d);
    return ;
}

