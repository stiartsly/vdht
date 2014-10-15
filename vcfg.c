#include "vglobal.h"
#include "vcfg.h"

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

static
int _vcfg_parse(struct vconfig* cfg, const char* filename)
{
#ifdef _STUB
    struct vcfg_item* item = NULL;
#endif
    vassert(cfg);
    vassert(filename);

#ifdef _STUB
    item = (struct vcfg_item*)malloc(sizeof(*item));
    vlog((!item), elog_malloc);
    retE((!item));

    vlist_init(&item->list);
    item->type = CFG_STR;
    item->key  = "route.db";
    item->val  = "route.db";
    item->nval = 0;

    vlist_add_tail(&cfg->items, &item->list);
#endif
    return 0;
}

static
int _vcfg_clear(struct vconfig* cfg)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    vassert(cfg);

    while(!vlist_is_empty(&cfg->items)) {
        node = vlist_pop_head(&cfg->items);
        item = vlist_entry(node, struct vcfg_item, list);
        free(item);
    }
    return 0;
}

static
int _vcfg_get_int(struct vconfig* cfg, const char* key, int* value)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

    __vlist_for_each(node, &cfg->items) {
        item = vlist_entry(node, struct vcfg_item, list);
        if ((!strcmp(item->key, key)) && (item->type == CFG_INT)) {
            *value = item->nval;
            ret = 0;
            break;
        }
    }
    return ret;
}

static
int _vcfg_get_str(struct vconfig* cfg, const char* key, char* value, int sz)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

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
    return ret;
}

static
struct vconfig_ops cfg_ops = {
    .parse   = _vcfg_parse,
    .clear   = _vcfg_clear,
    .get_int = _vcfg_get_int,
    .get_str = _vcfg_get_str
};

int vconfig_init(struct vconfig* cfg)
{
    vassert(cfg);

    vlist_init(&cfg->items);
    cfg->ops = &cfg_ops;
    return 0;
}

void vconfig_deinit(struct vconfig* cfg)
{
    vassert(cfg);

    cfg->ops->clear(cfg);
    return ;
}

