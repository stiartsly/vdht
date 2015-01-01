#include "vglobal.h"
#include "vcfg.h"

#define DEF_HOST_TICK_TMO       "5s"
#define DEF_NODE_TICK_INTERVAL  "7s"

#define DEF_ROUTE_DB_FILE       "route.db"
#define DEF_ROUTE_BUCKET_CAPC   ((int)10)
#define DEF_ROUTE_MAX_SND_TIMES ((int)5)
#define DEF_ROUTE_MAX_RCV_PERIOD "1m"
#define DEF_ROUTE_MAX_RECORD_PERIOD "5s"
#define DEF_LSCTL_UNIX_PATH     "/var/run/vdht/lsctl_socket"

#define DEF_DHT_PORT            ((int)12300)

#define DEF_SPY_CPU_CRITERIA    (5)
#define DEF_SPY_MEM_CRITERIA    (5)
#define DEF_SPY_IO_CRITERIA     (5)
#define DEF_SPY_UP_CRITERIA     (5)
#define DEF_SPY_DOWN_CRITERIA   (5)

#define DEF_SPY_CPU_FACTOR      (6)
#define DEF_SPY_MEM_FACTOR      (5)
#define DEF_SPY_IO_FACTOR       (2)
#define DEF_SPY_UP_FACTOR       (4)
#define DEF_SPY_DOWN_FACTOR     (4)

#define DEF_STUN_PORT           (13999)

enum {
    CFG_INT = 0,
    CFG_STR,
    CFG_UNKOWN
};

struct vcfg_item {
    struct vlist list;
    int   type;
    char* key;
    char* val;
    int   nval;
};

static MEM_AUX_INIT(cfg_item_cache, sizeof(struct vcfg_item), 8);
static
struct vcfg_item* vitem_alloc(void)
{
    struct vcfg_item* item = NULL;

    item = (struct vcfg_item*)vmem_aux_alloc(&cfg_item_cache);
    vlog((!item), elog_vmem_aux_alloc);
    retE_p((!item));
    return item;
}

static
void vitem_free(struct vcfg_item* item)
{
    vassert(item);
    if (item->key) {
        free(item->key);
    }
    if (item->val) {
        free(item->val);
    }
    vmem_aux_free(&cfg_item_cache, item);
    return ;
}

static
void vitem_init_int(struct vcfg_item* item, char* key, int nval)
{
    vassert(item);
    vassert(key);

    vlist_init(&item->list);
    item->type = CFG_INT;
    item->key  = key;
    item->val  = NULL;
    item->nval = nval;
    return ;
}

static
void vitem_init_str(struct vcfg_item* item, char* key, char* val)
{
    vassert(item);
    vassert(key);
    vassert(val);

    vlist_init(&item->list);
    item->type = CFG_STR;
    item->key  = key;
    item->val  = val;
    item->nval = 0;
    return ;
}

/*
 * strip underlines '_' from head and tail of string.
 */
static
int _strip_underline(char* str)
{
    char* cur = str;
    vassert(str);

    // strip '_' from the head of str.
    while(*cur == '_') {
        char* tmp = cur;
        while(*tmp != '\0') {
            *tmp = *(tmp + 1);
            tmp++;
        }
    }
    // strip '_' from the tail of str.
    cur += strlen(str) -1;
    while(*cur == '_') {
        *cur = '\0';
        cur--;
    }
    return 0;
}

static char last_section[64];
static
int eat_section_ln(struct vconfig* cfg, char* section_ln)
{
    char* cur = section_ln;
    int section_aten = 0;

    vassert(cfg);
    vassert(section_ln);

    cur++; //skip '['
    while (*cur != '\n') {
        switch(*cur) {
        case ' ':
        case '\t':
            retE((!section_aten));
            cur++;
            break;
        case ']': {
            retE((section_aten));
            memset(last_section, 0, 64);
            strncpy(last_section, section_ln + 1, cur-section_ln - 1);
            section_aten = 1;
            cur++;
            break;
        }
        case '0'...'9':
        case 'a'...'z':
        case 'A'...'Z':
            retE((section_aten));
            cur++;
            break;
        default:
            retE((1));
        }
    }
    cur++; // skip '\n'
    return (cur - section_ln);
}

static
int eat_comment_ln(struct vconfig* cfg, char* comm_ln)
{
    char* cur = (char*)comm_ln;

    vassert(cfg);
    vassert(comm_ln);

    cur++; // skip ';'
    while(*cur != '\n') cur++;
    cur++;

    return (cur - comm_ln);
}

static
int eat_param_ln(struct vconfig* cfg, char* param_ln)
{
    struct vcfg_item* item = NULL;
    char* cur = (char*)param_ln;
    char* val_pos = NULL;
    int key_aten = 0;
    int is_int   = 1;
    char* key = NULL;
    char* val = NULL;
    int  nval = 0;

    vassert(cfg);
    vassert(param_ln);

    while(*cur != '\n') {
        switch(*cur) {
        case '0'...'9':
            cur++;
            break;
        case 'a'...'z':
        case 'A'...'Z':
        case '.':
        case '/':
        case '_':
            cur++;
            if (key_aten) {
                is_int = 0;
            }
            break;
        case ' ':    //change whitepsace and tab to be '_'.
        case '\t':
            *cur = '_';
            cur++;
            retE((*cur == ' ' || *cur == '\t'));
            break;
        case '=': {
            int sz = 0;
            retE(key_aten);

            sz = cur - param_ln;
            sz += 1; // for '\0'
            sz += strlen(last_section);
            sz += 1; // for '.'

            key = malloc(sz);
            vlog((!key), elog_malloc);
            retE((!key));
            memset(key, 0, sz);

            strcpy(key, last_section);
            strcat(key, ".");
            strncat(key, param_ln, cur-param_ln);
            _strip_underline(key);

            key_aten = 1;
            val_pos = ++cur;
            break;
        }
        default:
            retE(1);
            break;
        }
    }

    { // meet line feed '\n'.
      // deal with value.
        char tmp[32];
        memset(tmp, 0, 32);
        strncpy(tmp, val_pos, cur - val_pos);
        _strip_underline(tmp);

        if (is_int) {
            errno = 0;
            nval = strtol(tmp, NULL, 10);
            retE((errno));
        } else {
            val = malloc(strlen(tmp) + 1);
            vlog((!val), elog_malloc);
            ret1E((!val), free(key));
            strcpy(val, tmp);
        }
    }
    cur++; //skip '\n'

    item = vitem_alloc();
    ret2E((!item), free(key), free(val));

    if (is_int) {
        vitem_init_int(item, key, nval);
    } else {
        vitem_init_str(item, key, val);
    }
    vlist_add_tail(&cfg->items, &item->list);
    return (cur - param_ln);
}

static
int _aux_parse(struct vconfig* cfg, void* buf)
{
    char* cur = (char*)buf;
    int ret = 0;

    vassert(cfg);
    vassert(buf);

    while(*cur != '\0') {
        switch(*cur) {
        case ' ':
        case '\t':
        case '\n':
        case '/':
            cur++;
            break;
        case '[':
            ret = eat_section_ln(cfg, cur);
            retE((ret < 0));
            cur += ret;
            break;
        case ';':
            ret = eat_comment_ln(cfg, cur);
            retE((ret < 0));
            cur += ret;
            break;
        case 'a' ... 'z':
        case 'A' ... 'Z':
        case '0' ... '9':
            ret = eat_param_ln(cfg, cur);
            retE((ret < 0));
            cur += ret;
            break;
        default:
            retE((1));
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
    int fd  = 0;
    int ret = 0;

    vassert(cfg);
    vassert(filename);

    fd = open(filename, O_RDONLY);
    vlog((fd < 0), elog_open);
    retE((fd < 0));

    memset(&stat, 0, sizeof(stat));
    ret = fstat(fd, &stat);
    vlog((ret < 0), elog_fstat);
    ret1E((ret < 0), close(fd));

    buf = malloc(stat.st_size + 1);
    vlog((!buf), elog_malloc);
    ret1E((!buf), close(fd));

    ret = read(fd, buf, stat.st_size);
    vlog((!buf), elog_read);
    vcall_cond((!buf), close(fd));
    vcall_cond((!buf), free(buf));
    retE((!buf));

    buf[stat.st_size] = '\0';
    close(fd);

    vlock_enter(&cfg->lock);
    ret = _aux_parse(cfg, buf);
    vlock_leave(&cfg->lock);
    retE((ret < 0));
    return 0;
}

/*
 * the routine to dump all config items.
 * @cfg:
 */
static
void _vcfg_dump(struct vconfig* cfg)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    vassert(cfg);

    vdump(printf("-> CONFIG"));
    vlock_enter(&cfg->lock);
    __vlist_for_each(node, &cfg->items) {
        item = vlist_entry(node, struct vcfg_item, list);
        switch(item->type) {
        case CFG_INT:
            vdump(printf("[%s]:%d", item->key, item->nval));
            break;
        case CFG_STR:
            vdump(printf("[%s]:%s", item->key, item->val));
            break;
        default:
            vassert(0);
            break;
        }
    }
    vlock_leave(&cfg->lock);
    vdump(printf("<- CONFIG"));
    return ;
}

/*
 * the routine to clear all config items
 * @cfg:
 */
static
int _vcfg_clear(struct vconfig* cfg)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    vassert(cfg);

    vlock_enter(&cfg->lock);
    while(!vlist_is_empty(&cfg->items)) {
        node = vlist_pop_head(&cfg->items);
        item = vlist_entry(node, struct vcfg_item, list);
        vitem_free(item);
    }
    vlock_leave(&cfg->lock);
    return 0;
}

/*
 * the routine to get value of config item by given kye @key.
 * @cfg:
 * @key:
 * @value:
 */
static
int _vcfg_get_int(struct vconfig* cfg, const char* key, int* value)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

    vassert(cfg);
    vassert(key);
    vassert(value);

    vlock_enter(&cfg->lock);
    __vlist_for_each(node, &cfg->items) {
        item = vlist_entry(node, struct vcfg_item, list);
        if ((!strcmp(item->key, key)) && (item->type == CFG_INT)) {
            *value = item->nval;
            ret = 0;
            break;
        }
    }
    vlock_leave(&cfg->lock);
    return ret;
}

/*
 * the routine to get value of config item by given kye @key.
 * @cfg:
 * @key:
 * @value:
 * @def_value:
 */
static
int _vcfg_get_int_ext(struct vconfig* cfg, const char* key, int* value, int def_value)
{
    struct vcfg_item* item = NULL;
    char* kay = NULL;
    int ret = 0;

    vassert(cfg);
    vassert(key);
    vassert(value);

    ret = cfg->ops->get_int(cfg, key, value);
    retS((ret >= 0));

    kay = (char*)malloc(strlen(key) + 1);
    retE((!kay));
    strcpy(kay, key);

    item = vitem_alloc();
    ret1E((!item), free(kay));
    vlist_init(&item->list);
    item->key = kay;
    item->val = NULL;
    item->nval = def_value;
    item->type = CFG_INT;

    vlock_enter(&cfg->lock);
    vlist_add_tail(&cfg->items, &item->list);
    vlock_leave(&cfg->lock);

    *value = def_value;
    return 0;
}

/*
 * the routine to get value of config item by given kye @key.
 * @cfg:
 * @key:
 * @value:
 * @sz:
 */
static
int _vcfg_get_str(struct vconfig* cfg, const char* key, char* value, int sz)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

    vassert(cfg);
    vassert(key);
    vassert(value);
    vassert(sz > 0);

    vlock_enter(&cfg->lock);
    __vlist_for_each(node, &cfg->items) {
        item = vlist_entry(node, struct vcfg_item, list);
        if ((!strcmp(item->key, key))
             && (item->type == CFG_STR)
             && (strlen(item->val) < sz)) {
            strcpy(value, item->val);
            ret = 0;
            break;
        }
    }
    vlock_leave(&cfg->lock);
    return ret;
}

/*
 * the routine to get value of config item by given kye @key.
 * @cfg:
 * @key:
 * @value:
 * @sz:
 * @def_value:
 */
static
int _vcfg_get_str_ext(struct vconfig* cfg, const char* key, char* value, int sz, char* def_value)
{
    struct vcfg_item* item = NULL;
    char* val = NULL;
    char* kay = NULL;
    int ret = 0;

    vassert(cfg);
    vassert(key);
    vassert(value);
    vassert(sz > 0);
    vassert(def_value);

    ret = cfg->ops->get_str(cfg, key, value, sz);
    retS((ret >= 0));
    retE((sz < strlen(def_value) + 1));

    val = (char*)malloc(strlen(def_value) + 1);
    retE((!val));
    strcpy(val, def_value);
    kay  = (char*)malloc(strlen(key) + 1);
    ret1E((!kay), free(val));
    strcpy(kay, key);

    item = vitem_alloc();
    ret2E((!item), free(val), free(kay));
    vlist_init(&item->list);
    item->key  = kay;
    item->val  = val;
    item->type = CFG_STR;
    item->nval = 0;

    vlock_enter(&cfg->lock);
    vlist_add_tail(&cfg->items, &item->list);
    vlock_leave(&cfg->lock);

    strcpy(value, def_value);
    return 0;
}

static
int _vcfg_check_section(struct vconfig* cfg, char* section)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int len = strlen(section);
    int found = 0;

    vassert(cfg);
    vassert(section);

    vlock_enter(&cfg->lock);
    __vlist_for_each(node, &cfg->items) {
        item = vlist_entry(node, struct vcfg_item, list);
        if ((len < strlen(item->key)) && !strncmp(section, item->key, len)) {
            found = 1;
            break;
        }
    }
    vlock_leave(&cfg->lock);
    return found;
}

static
struct vconfig_ops cfg_ops = {
    .parse       = _vcfg_parse,
    .clear       = _vcfg_clear,
    .dump        = _vcfg_dump,
    .get_int     = _vcfg_get_int,
    .get_int_ext = _vcfg_get_int_ext,
    .get_str     = _vcfg_get_str,
    .get_str_ext = _vcfg_get_str_ext,
    .check_section = _vcfg_check_section
};

static
int _vcfg_get_lsctl_unix_path(struct vconfig* cfg, char* path, int len)
{
    int ret = 0;

    vassert(cfg);
    vassert(path);
    vassert(len > 0);

    ret = cfg->ops->get_str_ext(cfg, "lsctl.unix_path", path, len, DEF_LSCTL_UNIX_PATH);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_host_tick_tmo(struct vconfig* cfg, int* tmo)
{
    char buf[32];
    int tms = 0;
    int ret = 0;

    vassert(cfg);
    vassert(tmo);

    memset(buf, 0, 32);
    ret = cfg->ops->get_str_ext(cfg, "global.tick_tmo", buf, 32, DEF_HOST_TICK_TMO);
    retE((ret < 0));

    ret = strlen(buf);
    switch(buf[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        retE((1));
        break;
    }
    errno = 0;
    ret = strtol(buf, NULL, 10);
    retE((errno));

    *tmo = (ret * tms);
    return 0;
}

static
int _vcfg_get_node_tick_interval(struct vconfig* cfg, int* interval)
{
    char buf[32];
    int tms = 0;
    int ret = 0;

    vassert(cfg);
    vassert(interval);

    memset(buf, 0, 32);
    ret = cfg->ops->get_str_ext(cfg, "node.tick_interval", buf, 32, DEF_NODE_TICK_INTERVAL);
    retE((ret < 0));

    ret = strlen(buf);
    switch(buf[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        retE((1));
    }
    errno = 0;
    ret = strtol(buf, NULL, 10);
    vlog((errno), elog_strtol);
    retE((errno));

    *interval = (ret * tms);
    return 0;
}

static
int _vcfg_get_route_db_file(struct vconfig* cfg, char* db, int len)
{
    int ret = 0;

    vassert(cfg);
    vassert(db);
    vassert(len > 0);

    ret = cfg->ops->get_str_ext(cfg, "route.db_file", db, len, DEF_ROUTE_DB_FILE);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_route_bucket_sz(struct vconfig* cfg, int* sz)
{
    int ret = 0;

    vassert(cfg);
    vassert(sz);

    ret = cfg->ops->get_int_ext(cfg, "route.bucket_sz", sz, DEF_ROUTE_BUCKET_CAPC);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_route_max_snd_tms(struct vconfig* cfg, int* tms)
{
    int ret = 0;

    vassert(cfg);
    vassert(tms);

    ret = cfg->ops->get_int_ext(cfg, "route.max_send_tims", tms, DEF_ROUTE_MAX_SND_TIMES);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_route_max_rcv_period(struct vconfig* cfg, int* period)
{
    char buf[32];
    int tms = 0;
    int ret = 0;

    vassert(cfg);
    vassert(period);

    memset(buf, 0, 32);
    ret = cfg->ops->get_str_ext(cfg, "route.max_rcv_period", buf, 32, DEF_ROUTE_MAX_RCV_PERIOD);
    retE((ret < 0));

    ret = strlen(buf);
    switch(buf[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        retE((1));
    }
    errno = 0;
    ret = strtol(buf, NULL, 10);
    vlog((errno), elog_strtol);
    retE((errno));

    *period = (ret * tms);
    return 0;
}

static
int _vcfg_get_route_max_record_period(struct vconfig* cfg, int* period)
{
    char buf[32];
    int tms = 0;
    int ret = 0;

    vassert(cfg);
    vassert(period);

    memset(buf, 0, 32);
    ret = cfg->ops->get_str_ext(cfg, "route.max_record_period", buf, 32, DEF_ROUTE_MAX_RECORD_PERIOD);
    retE((ret < 0));

    ret = strlen(buf);
    switch(buf[ret-1]) {
    case 's':
        tms = 1;
        break;
    case 'm':
        tms = 60;
        break;
    default:
        retE((1));
    }
    errno = 0;
    ret = strtol(buf, NULL, 10);
    vlog((errno), elog_strtol);
    retE((errno));

    *period = (ret * tms);
    return 0;
}

static
int _vcfg_get_dht_port(struct vconfig* cfg, int* port)
{
    int ret = 0;
    vassert(cfg);
    vassert(port);

    ret = cfg->ops->get_int_ext(cfg, "dht.port", port, DEF_DHT_PORT);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_cpu_criteria(struct vconfig* cfg, int* criteria)
{
    int ret = 0;
    vassert(cfg);
    vassert(criteria);

    ret = cfg->ops->get_int_ext(cfg, "spy.cpu_criteria", criteria, DEF_SPY_CPU_CRITERIA);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_mem_criteria(struct vconfig* cfg, int* criteria)
{
    int ret = 0;
    vassert(cfg);
    vassert(criteria);

    ret = cfg->ops->get_int_ext(cfg, "spy.mem_criteria", criteria, DEF_SPY_MEM_CRITERIA);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_io_criteria(struct vconfig* cfg, int* criteria)
{
    int ret = 0;
    vassert(cfg);
    vassert(criteria);

    ret = cfg->ops->get_int_ext(cfg, "spy.io_criteria", criteria, DEF_SPY_IO_CRITERIA);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_up_criteria(struct vconfig* cfg, int* criteria)
{
    int ret = 0;
    vassert(cfg);
    vassert(criteria);

    ret = cfg->ops->get_int_ext(cfg, "spy.up_criteria", criteria, DEF_SPY_UP_CRITERIA);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_down_criteria(struct vconfig* cfg, int* criteria)
{
    int ret = 0;
    vassert(cfg);
    vassert(criteria);

    ret = cfg->ops->get_int_ext(cfg, "spy.down_criteria", criteria, DEF_SPY_DOWN_CRITERIA);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_cpu_factor(struct vconfig* cfg, int* factor)
{
    int ret = 0;
    vassert(cfg);
    vassert(factor);

    ret = cfg->ops->get_int_ext(cfg, "spy.cpu_factor", factor, DEF_SPY_CPU_FACTOR);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_mem_factor(struct vconfig* cfg, int* factor)
{
    int ret = 0;
    vassert(cfg);
    vassert(factor);

    ret = cfg->ops->get_int_ext(cfg, "spy.mem_factor", factor, DEF_SPY_MEM_FACTOR);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_io_factor(struct vconfig* cfg, int* factor)
{
    int ret = 0;
    vassert(cfg);
    vassert(factor);

    ret = cfg->ops->get_int_ext(cfg, "spy.io_factor", factor, DEF_SPY_IO_FACTOR);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_up_factor(struct vconfig* cfg, int* factor)
{
    int ret = 0;
    vassert(cfg);
    vassert(factor);

    ret = cfg->ops->get_int_ext(cfg, "spy.up_factor", factor, DEF_SPY_UP_FACTOR);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_down_factor(struct vconfig* cfg, int* factor)
{
    int ret = 0;
    vassert(cfg);
    vassert(factor);

    ret = cfg->ops->get_int_ext(cfg, "spy.down_factor", factor, DEF_SPY_DOWN_FACTOR);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_ping_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.ping", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_ping_rsp_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.ping_rsp", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_find_node_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.find_node", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_find_node_rsp_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.find_node_rsp", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_find_closest_nodes_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.find_closest_nodes", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_finde_closest_nodes_rsp_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.find_closest_nodes_rsp", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_post_service_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.post_service", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_post_hash_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.post_hash", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_get_peers_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.get_peers", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_get_peers_rsp_cap(struct vconfig* cfg, int* on)
{
    char buf[32];
    int ret = 0;

    vassert(cfg);
    vassert(on);

    *on = 0;
    memset(buf, 0, 32);
    ret = cfg->ops->get_str(cfg, "proto.get_peers_rsp", buf, 32);
    retS((ret < 0));
    if (!strcmp(buf, "on")) {
        *on = 1;
    }
    return 0;
}

static
int _vcfg_get_stun_port(struct vconfig* cfg, int* port)
{
    int ret = 0;
    vassert(cfg);
    vassert(port);

    *port = 0;
    ret = cfg->ops->get_int_ext(cfg, "stun.port", port, DEF_STUN_PORT);
    retE((ret < 0));
    return 0;
}

static
int _vcfg_get_stun_server_name(struct vconfig* cfg, char* buf, int len)
{
    int ret = 0;

    vassert(cfg);
    vassert(buf);
    vassert(len > 0);

    ret = cfg->ops->get_str_ext(cfg, "stun.name", buf, len, "empty");
    retE((ret < 0));
    return 0;
}

static
struct vconfig_inst_ops cfg_inst_ops = {
    .get_lsctl_unix_path    = _vcfg_get_lsctl_unix_path,
    .get_host_tick_tmo      = _vcfg_get_host_tick_tmo,
    .get_node_tick_interval = _vcfg_get_node_tick_interval,

    .get_route_db_file      = _vcfg_get_route_db_file,
    .get_route_bucket_sz    = _vcfg_get_route_bucket_sz,
    .get_route_max_snd_tms  = _vcfg_get_route_max_snd_tms,
    .get_route_max_rcv_period    = _vcfg_get_route_max_rcv_period,
    .get_route_max_record_period = _vcfg_get_route_max_record_period,

    .get_dht_port           = _vcfg_get_dht_port,

    .get_cpu_criteria       = _vcfg_get_cpu_criteria,
    .get_mem_criteria       = _vcfg_get_mem_criteria,
    .get_io_criteria        = _vcfg_get_io_criteria,
    .get_up_criteria        = _vcfg_get_up_criteria,
    .get_down_criteria      = _vcfg_get_down_criteria,

    .get_cpu_factor         = _vcfg_get_cpu_factor,
    .get_mem_factor         = _vcfg_get_mem_factor,
    .get_io_factor          = _vcfg_get_io_factor,
    .get_up_factor          = _vcfg_get_up_factor,
    .get_down_factor        = _vcfg_get_down_factor,

    .get_ping_cap           = _vcfg_get_ping_cap,
    .get_ping_rsp_cap       = _vcfg_get_ping_rsp_cap,
    .get_find_node_cap      = _vcfg_get_find_node_cap,
    .get_find_node_rsp_cap  = _vcfg_get_find_node_rsp_cap,
    .get_find_closest_nodes_cap     = _vcfg_get_find_closest_nodes_cap,
    .get_find_closest_nodes_rsp_cap = _vcfg_get_finde_closest_nodes_rsp_cap,
    .get_post_service_cap   = _vcfg_get_post_service_cap,
    .get_post_hash_cap      = _vcfg_get_post_hash_cap,
    .get_get_peers_cap      = _vcfg_get_get_peers_cap,
    .get_get_peers_rsp_cap  = _vcfg_get_get_peers_rsp_cap,

    .get_stun_port          = _vcfg_get_stun_port,
    .get_stun_server_name   = _vcfg_get_stun_server_name
};

int vconfig_init(struct vconfig* cfg)
{
    vassert(cfg);

    vlist_init(&cfg->items);
    vlock_init(&cfg->lock);
    cfg->ops      = &cfg_ops;
    cfg->inst_ops = &cfg_inst_ops;
    return 0;
}

void vconfig_deinit(struct vconfig* cfg)
{
    vassert(cfg);

    cfg->ops->clear(cfg);
    vlock_deinit(&cfg->lock);
    return ;
}

