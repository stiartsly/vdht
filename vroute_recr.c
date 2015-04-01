#include "vglobal.h"
#include "vroute.h"

struct vrecord {
    struct vlist list;
    vtoken token;
    time_t snd_ts;
};

static MEM_AUX_INIT(record_cache, sizeof(struct vrecord), 4);
static
struct vrecord* vrecord_alloc(void)
{
    struct vrecord* record = NULL;

    record = (struct vrecord*)vmem_aux_alloc(&record_cache);
    vlogE_cond((!record), elog_vmem_aux_alloc);
    retE_p((!record));
    return record;
}

static
void vrecord_free(struct vrecord* record)
{
    vassert(record);
    vmem_aux_free(&record_cache, record);
    return ;
}

static
void vrecord_init(struct vrecord* record, vtoken* token)
{
    vassert(record);
    vassert(token);

    vlist_init(&record->list);
    vtoken_copy(&record->token, token);
    record->snd_ts = time(NULL);
    return ;
}

static
void vrecord_dump(struct vrecord* record)
{
    vassert(record);
    vtoken_dump(&record->token);
    vdump(printf("timestamp[snd]: %s",  ctime(&record->snd_ts)));
    return ;
}

/*
 * the routine to make a record after sending a dht query msg.
 * in case to check the rightness of response message.
 *
 * @space:
 * @token:
 */
static
int _vroute_recr_space_make(struct vroute_recr_space* space, vtoken* token)
{
    struct vrecord* record = NULL;

    vassert(space);
    vassert(token);

    record = vrecord_alloc();
    vlogE_cond((!record), elog_vrecord_alloc);
    retE((!record));

    vrecord_init(record, token);
    vlock_enter(&space->lock);
    vlist_add_tail(&space->records, &record->list);
    vlock_leave(&space->lock);

    return 0;
}

/*
 * the routine to check the dht message( always response message) with given
 * token is in the messages record that sent before.
 *
 * @space:
 * @token:
 */
static
int _vroute_recr_space_check(struct vroute_recr_space* space, vtoken* token)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(space);
    vassert(token);

    vlock_enter(&space->lock);
    __vlist_for_each(node, &space->records) {
        record = vlist_entry(node, struct vrecord, list);
        if (vtoken_equal(&record->token, token)) {
            vlist_del(&record->list);
            vrecord_free(record);
            found = 1;
            break;
        }
    }
    vlock_leave(&space->lock);
    return found;
}

/*
 * the routine to reap all timeout message records that menas all messages to
 * those records were not reachable.
 *
 * @space:
 */
static
void _vroute_recr_space_timed_reap(struct vroute_recr_space* space)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    time_t now = time(NULL);
    vassert(space);

    vlock_enter(&space->lock);
    __vlist_for_each(node, &space->records) {
        record = vlist_entry(node, struct vrecord, list);
        if ((now - record->snd_ts) > space->max_recr_period) {
            vlist_del(&record->list);
            vrecord_free(record);
        }
    }
    vlock_leave(&space->lock);
    return ;
}

/*
 * the routine to clear all message records
 * @space:
 */
static
void _vroute_recr_space_clear(struct vroute_recr_space* space)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    vassert(space);

    vlock_enter(&space->lock);
    while(!vlist_is_empty(&space->records)) {
        node = vlist_pop_head(&space->records);
        record = vlist_entry(node, struct vrecord, list);
        vrecord_free(record);
    }
    vlock_leave(&space->lock);
    return ;
}

/*
 * the routine to dump all message record infomartion
 *
 * @space:
 */
static
void _vroute_recr_space_dump(struct vroute_recr_space* space)
{
    struct vrecord* record = NULL;
    struct vlist* node = NULL;
    vassert(space);

    vlock_enter(&space->lock);
    __vlist_for_each(node, &space->records) {
        record = vlist_entry(node, struct vrecord, list);
        vrecord_dump(record);
    }
    vlock_leave(&space->lock);
    return ;
}

static
struct vroute_recr_space_ops route_record_space_ops = {
    .make        = _vroute_recr_space_make,
    .check       = _vroute_recr_space_check,
    .timed_reap  = _vroute_recr_space_timed_reap,
    .clear       = _vroute_recr_space_clear,
    .dump        = _vroute_recr_space_dump
};

int vroute_recr_space_init(struct vroute_recr_space* space)
{
    vassert(space);

    space->max_recr_period = 5; //5s;
    vlist_init(&space->records);
    vlock_init(&space->lock);

    space->ops = &route_record_space_ops;
    return 0;
}

void vroute_recr_space_deinit(struct vroute_recr_space* space)
{
    vassert(space);

    space->ops->clear(space);
    vlock_deinit(&space->lock);

    return ;
}

