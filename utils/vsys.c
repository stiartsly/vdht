#include "vglobal.h"
#include "vsys.h"

/*
 * for pthread mutex
 */
int vlock_init(struct vlock* lock)
{
    vassert(lock);
    pthread_mutexattr_init(&lock->attr);
    pthread_mutexattr_settype(&lock->attr, PTHREAD_MUTEX_RECURSIVE_NP);
    return pthread_mutex_init(&lock->mutex, &lock->attr);
}

int vlock_enter(struct vlock* lock)
{
    vassert(lock);
    return pthread_mutex_lock(&lock->mutex);
}

int vlock_leave(struct vlock* lock)
{
    vassert(lock);
    return pthread_mutex_unlock(&lock->mutex);
}

void vlock_deinit(struct vlock* lock)
{
    vassert(lock);
    pthread_mutex_destroy(&lock->mutex);
    pthread_mutexattr_destroy(&lock->attr);
    return ;
}

/*
 * for pthread condition
 */
int vcond_init(struct vcond* cond)
{
    int res = 0;
    vassert(cond);

    res = pthread_cond_init(&cond->cond, 0);
    retE((res < 0));
    return 0;
}

int vcond_wait(struct vcond* cond, struct vlock* lock)
{
    int res = 0;
    vassert(cond);
    vassert(lock);

    res = pthread_cond_wait(&cond->cond, &lock->mutex);
    retE((res < 0));
    return 0;
}

int vcond_signal(struct vcond* cond)
{
    int res = 0;
    vassert(cond);

    res = pthread_cond_signal(&cond->cond);
    retE((res < 0));
    return 0;
}

void vcond_deinit(struct vcond* cond)
{
    vassert(cond);
    pthread_cond_destroy(&cond->cond);
    return ;
}


static
void* _aux_entry(void* argv)
{
    struct vthread* thread = (struct vthread*)argv;
    pthread_mutex_lock(&thread->mutex);
    pthread_mutex_unlock(&thread->mutex);

    thread->entry_cb(thread->cookie);
    thread->quited = 1;
    return thread;
}
/*
 * for thread
 */
int vthread_init(struct vthread* thread, vthread_entry_t entry, void* argv)
{
    int res = 0;
    vassert(thread);
    vassert(entry);

    pthread_mutex_init(&thread->mutex, 0);
    thread->entry_cb = entry;
    thread->cookie = argv;
    thread->quited  = 0;
    thread->started = 0;

    pthread_mutex_lock(&thread->mutex);
    res = pthread_create(&thread->thread, 0, _aux_entry, thread);
    vlog((res), elog_pthread_create);
    if (res) {
        pthread_mutex_unlock(&thread->mutex);
        pthread_mutex_destroy(&thread->mutex);
        return -1;
    }
    return 0;
}

int vthread_start(struct vthread* thread)
{
    vassert(thread);

    vlog((thread->started), printf("#!thread already started"));
    retE((thread->started));
    thread->started = 1;
    pthread_mutex_unlock(&thread->mutex);
    return 0;
}

void vthread_deinit(struct vthread* thread)
{
    vassert(thread);

    vlog((!thread->started),printf("#!thread not started yet"));
    vlog((!thread->quited), printf("#!thread not quited yet"));
    pthread_mutex_destroy(&thread->mutex);
//    pthread_destroy(&thread->thread);
    return ;
}

/*
 * for vtimer
 */

static
void timer_thread(union sigval sv)
{
    struct vtimer* timer = (struct vtimer*)(sv.sival_int);
    vassert(timer);

    (void)timer->cb(timer->cookie);
    return ;
}

int vtimer_init(struct vtimer* timer, vtimer_cb_t cb, void* cookie)
{
    struct sigevent evp;
    int ret = 0;

    vassert(timer);
    vassert(cb);

    timer->id = (timer_t)-1;
    timer->cb = cb;
    timer->cookie = cookie;

    memset(&evp, 0, sizeof(evp));
    evp.sigev_value.sival_int = (int)timer;
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_notify_function = timer_thread;

    ret = timer_create(CLOCK_REALTIME, &evp, &timer->id);
    vlog((ret < 0), elog_timer_create);
    retE((ret < 0));
    return 0;
}

int vtimer_start(struct vtimer* timer, int timeout)
{
    struct itimerspec tmo;
    int ret = 0;

    vassert(timer);
    vassert(timeout > 0);
    vassert(timer->id > 0);

    tmo.it_interval.tv_sec = timeout;
    tmo.it_interval.tv_nsec = 0;
    tmo.it_value.tv_sec = timeout;
    tmo.it_value.tv_nsec = 0;

    ret = timer_settime(timer->id, 0, &tmo, NULL);
    vlog((ret < 0), elog_timer_settime);
    retE((ret < 0));
    return 0;
}

int vtimer_restart(struct vtimer* timer, int timeout)
{
    struct itimerspec tmo;
    int ret = 0;

    vassert(timer);
    vassert(timeout > 0);
    vassert(timer->id > 0);

    tmo.it_interval.tv_sec = timeout;
    tmo.it_interval.tv_nsec = 0;
    tmo.it_value.tv_sec = timeout;
    tmo.it_value.tv_nsec = 0;

    ret = timer_settime(timer->id, 0, &tmo, NULL);
    vlog((ret < 0), elog_timer_settime);
    retE((ret < 0));
    return 0;
}

int vtimer_stop(struct vtimer* timer)
{
    int ret = 0;
    vassert(timer);
    vassert(timer->id > 0);

    ret = timer_delete(timer->id);
    vlog((ret < 0), elog_timer_delete);
    retE((ret < 0));
    timer->id = (timer_t)-1;
    return 0;
}

void vtimer_deinit(struct vtimer* timer)
{
    vassert(timer);

    //should call "stop" before "deinit".
    if (timer->id > 0) {
        timer_delete(timer->id);
        timer->id = (timer_t)-1;
    }
    return ;
}

