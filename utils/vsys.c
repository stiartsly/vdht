#include "vglobal.h"
#include "vsys.h"

/*
 * for pthread mutex
 */
int vlock_init(struct vlock* lock)
{
    vassert(lock);
    return pthread_mutex_init(&lock->mutex, 0);
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


/*
 * for thread
 */
int vthread_init(struct vthread* thread, vthread_entry_t entry, void* argv)
{
    int res = 0;
    vassert(thread);
    vassert(entry);

    //todo;
    res = pthread_create(&thread->thread, 0, (void* (*)(void*))entry, argv);
    retE((res));
    return 0;
}

int vthread_start(struct vthread* thread)
{
    vassert(thread);
    //todo;
    return 0;
}

void vthread_deinit(struct vthread* thread)
{
    vassert(thread);
    //todo;
    return ;
}

/*
 * for vtimer
 */

int vtimer_init(struct vtimer* timer, vtimer_cb_t cb, void* cookie)
{
    vassert(timer);
    vassert(cb);

    timer->cb = cb;
    timer->cookie = cookie;
    // use system api @timer_create .
    //todo;
    return 0;
}

int vtimer_start(struct vtimer* timer, int timeout)
{
    vassert(timer);
    vassert(timeout > 0);
    //todo;
    return 0;
}

int vtimer_restart(struct vtimer* timer, int timeout)
{
    vassert(timer);
    vassert(timeout > 0);
    //todo;
    return 0;
}

int vtimer_stop(struct vtimer* timer)
{
    vassert(timer);
    //todo;
    return 0;
}

void vtimer_deinit(struct vtimer* timer)
{
    vassert(timer);
    // use @timer_delete api .
    //todo;
    return ;
}

