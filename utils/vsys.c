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
void* _aux_thread_entry(void* argv)
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
    res = pthread_create(&thread->thread, 0, _aux_thread_entry, thread);
    vlogEv((res), elog_pthread_create);
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

    vlogEv((thread->started), "#!thread already started");
    retE((thread->started));
    thread->started = 1;
    pthread_mutex_unlock(&thread->mutex);
    return 0;
}

int vthread_join(struct vthread* thread, int* quit_code)
{
    int ret = 0;

    vassert(thread);
    vassert(quit_code);

    ret = pthread_join(thread->thread, (void**)quit_code);
    retE((ret < 0));
    return 0;
}

void vthread_deinit(struct vthread* thread)
{
    vassert(thread);

    vlogEv((!thread->started),"#!thread not started yet");
    vlogEv((!thread->quited), "#!thread not quited yet");
    pthread_mutex_destroy(&thread->mutex);
    return ;
}

/*
 * for vtimer
 */

static
void _aux_timer_routine(union sigval sv)
{
    struct vtimer* timer = (struct vtimer*)(sv.sival_ptr);
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
    evp.sigev_value.sival_ptr = timer;
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_notify_function = _aux_timer_routine;

    ret = timer_create(CLOCK_REALTIME, &evp, &timer->id);
    vlogEv((ret < 0), elog_timer_create);
    retE((ret < 0));
    return 0;
}

int vtimer_start(struct vtimer* timer, int timeout)
{
    struct itimerspec tmo;
    int ret = 0;

    vassert(timer);
    vassert(timeout > 0);
    vassert(timer->id != (timer_t)-1);

    tmo.it_interval.tv_sec = timeout;
    tmo.it_interval.tv_nsec = 0;
    tmo.it_value.tv_sec = timeout;
    tmo.it_value.tv_nsec = 0;

    ret = timer_settime(timer->id, 0, &tmo, NULL);
    vlogEv((ret < 0), elog_timer_settime);
    retE((ret < 0));
    return 0;
}

int vtimer_restart(struct vtimer* timer, int timeout)
{
    struct itimerspec tmo;
    int ret = 0;

    vassert(timer);
    vassert(timeout > 0);
    vassert(timer->id != (timer_t)-1);

    tmo.it_interval.tv_sec = timeout;
    tmo.it_interval.tv_nsec = 0;
    tmo.it_value.tv_sec = timeout;
    tmo.it_value.tv_nsec = 0;

    ret = timer_settime(timer->id, 0, &tmo, NULL);
    vlogEv((ret < 0), elog_timer_settime);
    retE((ret < 0));
    return 0;
}

int vtimer_stop(struct vtimer* timer)
{
    int ret = 0;
    vassert(timer);
    vassert(timer->id != (timer_t)-1);

    ret = timer_delete(timer->id);
    vlogEv((ret < 0), elog_timer_delete);
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

int vsys_get_cpu_ratio(int* ratio)
{
    int user   = 0;
    int system = 0;
    int nice   = 0;
    int idle   = 0;
    int occupy = 0;
    char buf[128];
    char tmp[32];
    int fd  = 0;
    int ret = 0;
    vassert(ratio);

    fd = open("/proc/stat", O_RDONLY);
    vlogEv((fd < 0), elog_open);
    retE((fd < 0));
    memset(buf, 0, 128);
    ret = read(fd, buf, 128);
    close(fd);
    retE((ret < 0));

    memset(tmp, 0, 32);
    ret = sscanf(buf, "%s%u%u%u%u", tmp, &user, &nice, &system, &idle);
    retE((ret < 0));
    occupy = user + nice + system;
    *ratio = (int)(occupy * 10) / (occupy + idle);
    return 0;
}

int vsys_get_mem_ratio(int* ratio)
{
    int total = 0;
    int mfree = 0;
    char buf[128];
    char tmp[32];
    int ret = 0;
    int fd  = 0;
    vassert(ratio);

    fd = open("/proc/meminfo", O_RDONLY);
    vlogEv((fd < 0), elog_open);
    retE((ret < 0));
    memset(buf, 0, 128);
    ret = read(fd, buf, 128);
    close(fd);
    retE((ret < 0));

    memset(tmp, 0,32);
    ret = sscanf(buf, "%s%u%s%s%u", tmp, &total, tmp, tmp, &mfree);
    retE((ret < 0));
    *ratio = (int)(((total - mfree) * 10)/total);
    return 0;
}

int vsys_get_io_ratio (int* ratio)
{
    vassert(ratio);
    //todo;
    return 0;
}

int vsys_get_net_ratio(int* up_ratio, int* down_ratio)
{
    vassert(up_ratio);
    vassert(down_ratio);

    //todo;
    return 0;
}

