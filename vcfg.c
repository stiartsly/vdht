#include "vglobal.h"
#include "vcfg.h"

enum {
    VCFG_INT = 0,
    VCFG_STR,
    VCFG_BUTT
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
        if ((!strcmp(item->key, key)) && (item->type == VCFG_INT)) {
            *value = item->nval;
            ret = 0;
            break;
        }
    }
    return ret;
}

int _vcfg_get_str(const char* key, char* value, int size)
{
    struct vcfg_item* item = NULL;
    struct vlist* node = NULL;
    int ret = -1;

    __vlist_for_each(node, &gcfg.items) {
        item = vlist_entry(node, struct vcfg_item, list);
        if ((!strcmp(item->key, key))
             && (item->type == VCFG_STR)
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
    vassert(cfg_file);

    vlist_init(&gcfg);

    //todo;
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

