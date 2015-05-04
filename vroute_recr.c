#include "vglobal.h"
#include "vroute.h"

struct vrecord {
    vtoken token;
    time_t snd_ts;
};

static MEM_AUX_INIT(record_cache, sizeof(struct vrecord), 4);
static
struct vrecord* vrecord_alloc(void)
{
    struct vrecord* record = NULL;

    record = (struct vrecord*)vmem_aux_alloc(&record_cache);
    vlogEv((!record), elog_vmem_aux_alloc);
    retE_p((!record));
    memset(record, 0, sizeof(*record));
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
    vlogEv((!record), elog_vrecord_alloc);
    retE((!record));

    vrecord_init(record, token);
    varray_add_tail(&space->records, record);

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
    int found = 0;
    int i = 0;

    vassert(space);
    vassert(token);

    for (i = 0; i < varray_size(&space->records); i++) {
        record = (struct vrecord*)varray_get(&space->records, i);
        if (vtoken_equal(&record->token, token)) {
            varray_del(&space->records, i);
            vrecord_free(record);
            found = 1;
            break;
        }
    }
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
    time_t now = time(NULL);
    int i = 0;
    vassert(space);

    for (i = 0; i < varray_size(&space->records);) {
        record = (struct vrecord*)varray_get(&space->records, i);
        if ((now - record->snd_ts) > space->max_recr_period) {
            varray_del(&space->records, i);
            vrecord_free(record);
        } else {
            i++;
        }
    }
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
    vassert(space);

    while (varray_size(&space->records)) {
        record = (struct vrecord*)varray_pop_tail(&space->records);
        vrecord_free(record);
    }
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
    int i = 0;
    vassert(space);

    for (i = 0; i < varray_size(&space->records); i++) {
        record = (struct vrecord*)varray_get(&space->records, i);
        vrecord_dump(record);
    }
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
    varray_init(&space->records, 8);

    space->ops = &route_record_space_ops;
    return 0;
}

void vroute_recr_space_deinit(struct vroute_recr_space* space)
{
    vassert(space);

    space->ops->clear(space);
    varray_deinit(&space->records);

    return ;
}

