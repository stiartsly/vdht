#include "vglobal.h"
#include "vmsger.h"

void* vmsg_buf_alloc(int sz)
{
    void* buf = NULL;
    int bsz = BUF_SZ;

    if (sz > 0) {
        bsz = sz * BUF_SZ;
    }

    buf = malloc(bsz);
    vlogEv((!buf), elog_malloc);
    retE_p((!buf));

    memset(buf, 0, bsz);
    return buf;
}

void vmsg_buf_free(void* buf)
{
    if (buf) {
        free(buf);
    }
    return ;
}

/*
 * auxiliary funcs for vmsg_sys.
 */
static MEM_AUX_INIT(ms_cache, sizeof(struct vmsg_sys), 8);
struct vmsg_sys* vmsg_sys_alloc(int sz)
{
    struct vmsg_sys* ms = NULL;
    void* buf = NULL;

    vassert(sz >= 0);

    ms = (struct vmsg_sys*)vmem_aux_alloc(&ms_cache);
    vlogEv((!ms), elog_vmem_aux_alloc);
    retE_p((!ms));
    memset(ms, 0, sizeof(*ms));

    if (sz > 0) {
        buf = malloc(sz);
        vlogEv((!buf), elog_malloc);
        ret1E_p((!buf), vmem_aux_free(&ms_cache, ms));
        memset(buf, 0, sz);
    }
    ms->data = buf;
    ms->len  = sz;

    return ms;
}

void vmsg_sys_free(struct vmsg_sys* ms)
{
    vassert(ms);

    if (ms->data) {
        free(ms->data);
    }
    vmem_aux_free(&ms_cache, ms);

    return ;
}

void vmsg_sys_init(struct vmsg_sys* ms, struct vsockaddr* addr, struct vsockaddr* spec, int len, void* data)
{
    vassert(ms);
    vassert(addr || !addr);
    vassert(len > 0);
    vassert(data);

    vlist_init(&ms->list);
    if (addr) {
        memcpy(&ms->addr, addr, sizeof(*addr));
    }
    if (spec) {
        memcpy(&ms->spec, spec, sizeof(*spec));
    }
    ms->len  = len;
    ms->data = data;

    return ;
}

void vmsg_sys_refresh(struct vmsg_sys* ms, int buf_len)
{
    void* data = ms->data;
    vassert(ms);
    vassert(buf_len > 0);

    memset(data, 0, buf_len);
    memset(ms,   0, sizeof(*ms));

    vlist_init(&ms->list);
    ms->len  = buf_len;
    ms->data = data;
    return ;
}

/*
 * auxiliary func for vmsg_usr.
 */
void vmsg_usr_init(struct vmsg_usr* mu, int msgId, struct vsockaddr* addr, struct vsockaddr* spec, int len, void* data)
{
    vassert(mu);
    vassert(msgId >= 0);
    vassert(addr);
    vassert(len > 0);
    vassert(data);

    mu->msgId = msgId;
    mu->addr  = addr;
    mu->spec  = spec;
    mu->len   = len;
    mu->data  = data;
    return ;
}

/*
 * auxiliary funcs for vmsg_cb
 */
static MEM_AUX_INIT(mcb_cache, sizeof(struct vmsg_cb), 0);
struct vmsg_cb* vmsg_cb_alloc(void)
{
    struct vmsg_cb* mcb = NULL;

    mcb = (struct vmsg_cb*)vmem_aux_alloc(&mcb_cache);
    vlogEv((!mcb), elog_vmem_aux_alloc);
    retE_p((!mcb));
    return mcb;
}

void vmsg_cb_free(struct vmsg_cb* mcb)
{
    vassert(mcb);
    vmem_aux_free(&mcb_cache, mcb);
    return ;
}

void vmsg_cb_init(struct vmsg_cb* mcb, int msgId, vmsg_cb_t cb, void* cookie)
{
    vassert(mcb);
    vassert(msgId > 0);
    vassert(cb);
    vassert(cookie);

    vlist_init(&mcb->list);
    mcb->msgId = msgId;
    mcb->cb    = cb;
    mcb->cookie= cookie;
    return ;
}

/*
 * when rpc receives a msg from socket, it calls dispatch callbck routing
 * (@msger->ops->dsptch)to handle the received msg.
 *
 * @msger:
 * @msg:  the msg received for system format.
 */
static
int _vmsger_dsptch(struct vmsger* msger, struct vmsg_sys* msg)
{
    struct vmsg_usr usr_msg;
    struct vmsg_cb* mcb = NULL;
    struct vlist* node = NULL;
    int ret = 0;

    vassert(msger);
    vassert(msg);
    vassert(msger->unpack_cb);
    vassert(msger->cookie2);

    ret = msger->unpack_cb(msger->cookie2, msg, &usr_msg);
    retE((ret < 0));

    vlock_enter(&msger->lock_cbs);
    __vlist_for_each(node, &msger->cbs) {
        mcb = vlist_entry(node, struct vmsg_cb, list);
        if (mcb->msgId == usr_msg.msgId) {
            mcb->cb(mcb->cookie, &usr_msg);
        }
    }
    vlock_leave(&msger->lock_cbs);
    return 0;
}

/*
 * each user msg has it's own routine(or callback) to deal with.
 * so, before recoginizing msg, it's certain callback need to be
 * registered.
 *
 * @msger:
 * @cookie: cookie for cb
 * @cb:     routine to handle the msg with msgId(@id)
 * @id:     msgId.
 */
static
int _vmsger_add_cb(struct vmsger* msger, void* cookie, vmsg_cb_t cb, int id)
{
    struct vmsg_cb* mcb = NULL;
    struct vlist* node = NULL;
    int found = 0;

    vassert(msger);
    vassert(cb);
    vassert(id > 0);
    vassert(cookie);

    vlock_enter(&msger->lock_cbs);
    __vlist_for_each(node, &msger->cbs) {
        mcb = vlist_entry(node, struct vmsg_cb, list);
        if (mcb->msgId == id) {
            found = 1;
            break;
        }
    }
    if (!found) {
        mcb = vmsg_cb_alloc();
        vlogEv((!mcb), elog_vmsg_cb_alloc);
        ret1E((!mcb), vlock_leave(&msger->lock_cbs));
        vmsg_cb_init(mcb, id, cb, cookie);
        vlist_add_tail(&msger->cbs, &mcb->list);
    }
    vlock_leave(&msger->lock_cbs);
    return 0;
}

/*
 *  when msg is about to send, it need to be put in msg queue in
 *  mapping msger. Then the msger mechanism take over the msg sending
 *  work. see (vrpc->ops->select, vroute->ops->loop)
 *
 *  @msger:
 *  @mu:  user message body.
 */
static
int _vmsger_push(struct vmsger* msger, struct vmsg_usr* mu)
{
    struct vmsg_sys* ms = NULL;
    int ret = 0;

    vassert(msger);
    vassert(mu);

    ms = vmsg_sys_alloc(0);
    vlogEv((!ms), elog_vmsg_sys_alloc);
    retE((!ms));

    ret = msger->pack_cb(msger->cookie1, mu, ms);
    ret1E((ret < 0), vmsg_sys_free(ms));

    vlock_enter(&msger->lock_msgs);
    vlist_add_tail(&msger->msgs, &ms->list);
    vlock_leave(&msger->lock_msgs);

    return 0;
}

/*
 *  to check whether msger has message to send.
 *
 * @msger:
 */
static
int _vmsger_popable(struct vmsger* msger)
{
    int ret = 0;
    vassert(msger);

    vlock_enter(&msger->lock_msgs);
    ret = !vlist_is_empty(&msger->msgs);
    vlock_leave(&msger->lock_msgs);

    return ret;
}

/*
 *  msg will be send by rpc as soon as rpc turns to be writeable.
 *  rpc fetch a msg from mapping msger to send.
 *  fecth policy: FIFO
 *
 * @msger:
 * @msg:  [out] msg to send, which is fetched from msger queue.
 */
static
int _vmsger_pop(struct vmsger* msger, struct vmsg_sys** ms)
{
    struct vlist* node = NULL;
    vassert(msger);
    vassert(ms);

    *ms = NULL;
    vlock_enter(&msger->lock_msgs);
    if (!vlist_is_empty(&msger->msgs)) {
        node = vlist_pop_head(&msger->msgs);
        *ms = vlist_entry(node, struct vmsg_sys, list);
    }
    vlock_leave(&msger->lock_msgs);

    return (*ms) ? 0 : -1;
}

/*
 * to clear msger's all contents.
 * @msger:
 */
static
int _vmsger_clear(struct vmsger* msger)
{
    struct vmsg_cb*  mcb = NULL;
    struct vmsg_sys* ms  = NULL;
    struct vlist*   node = NULL;
    vassert(msger);

    vlock_enter(&msger->lock_cbs);
    while(!vlist_is_empty(&msger->cbs)) {
        node = vlist_pop_head(&msger->cbs);
        mcb  = vlist_entry(node, struct vmsg_cb, list);
        vmsg_cb_free(mcb);
    }
    vlock_leave(&msger->lock_cbs);

    vlock_enter(&msger->lock_msgs);
    while(!vlist_is_empty(&msger->msgs)) {
        node = vlist_pop_head(&msger->msgs);
        ms   = vlist_entry(node, struct vmsg_sys, list);
        vmsg_sys_free(ms);
    }
    vlock_leave(&msger->lock_msgs);
    return 0;
}

/*
 * @msger:
 */
static
int _vmsger_dump(struct vmsger* msger)
{
    vassert(msger);
    //todo;
    return 0;
}

static
struct vmsger_ops msger_ops = {
    .dsptch  = _vmsger_dsptch,
    .add_cb  = _vmsger_add_cb,
    .push    = _vmsger_push,
    .popable = _vmsger_popable,
    .pop     = _vmsger_pop,
    .clear   = _vmsger_clear,
    .dump    = _vmsger_dump
};

int vmsger_init(struct vmsger* msger)
{
    vassert(msger);

    memset(msger, 0, sizeof(*msger));
    vlist_init(&msger->cbs);
    vlist_init(&msger->msgs);
    vlock_init(&msger->lock_cbs);
    vlock_init(&msger->lock_msgs);

    msger->ops = &msger_ops;
    return 0;
}

void vmsger_deinit(struct vmsger* msger)
{
    vassert(msger);

    msger->ops->clear(msger);
    vlock_deinit(&msger->lock_cbs);
    vlock_deinit(&msger->lock_msgs);
    return ;
}

void vmsger_reg_pack_cb(struct vmsger* msger, vmsger_pack_t cb, void* cookie)
{
    vassert(msger);
    vassert(cb);

    msger->pack_cb = cb;
    msger->cookie1 = cookie;
    return;
}

void vmsger_reg_unpack_cb(struct vmsger* msger, vmsger_unpack_t cb, void* cookie)
{
    vassert(msger);
    vassert(cb);

    msger->unpack_cb = cb;
    msger->cookie2   = cookie;
    return;
}

