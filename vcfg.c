#include "vglobal.h"
#include "vcfg.h"

enum {
    CFG_INT = 0,
    CFG_STR,
    CFG_UNKOWN
};

struct vcfg_item {
    struct list list;
    int   type;
    char* key;
    char* val;
    int   nval;
};

static struct vlist gcfg;
int _vcfg_get_int(const char* key, int* value)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

    __vlist_for_each(node, &gcfg.items) {
        item = vlist_entry(node, struct vcfg_item, list);
        if ((!strcmp(item->key, key)) && (item->type == CFG_INT)) {
            *value = item->nval;
            ret = 0;
            break;
        }
    }
    return ret;
}

int _vcfg_get_str(const char* key, char* value, int sz)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

    __vlist_for_each(node, &gcfg.items) {
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

struct vcfg_ops cfg_ops = {
    .get_int = _vcfg_get_int,
    .get_str = _vcfg_get_str
};

int vcfg_open(const char* cfg_file)
{
    struct vcfg_item* item = NULL;
    vassert(cfg_file);

    vlist_init(&gcfg);

#if _STUB
    item = (struct vcfg_item*)malloc(sizeof(*item));
    vlog_cond((!item), elog_malloc);
    retE((!item));

    vlist_init(&item->list);
    item->type = CFG_STR;
    item->key  = "route.db";
    item->val  = "route.db"
    item->nval = 0;

    vlist_add_tail(&gcfg, &item->list);
#endif
    return 0;
}

void vcfg_close()
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;

    while(!vlist_is_empty(&gcfg);
        node = vlist_pop_head(&gcfg);
        item = vlist_entry(node, struct vcfg_item, list);
        free(item);
    }
    return ;
}

