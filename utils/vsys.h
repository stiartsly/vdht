#ifndef __VSYS_H__
#define __VSYS_H__

#include <pthread.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>

/*
 * vlock
 */
struct vlock {
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
};

extern int  vlock_init  (struct vlock*);
extern int  vlock_enter (struct vlock*);
extern int  vlock_leave (struct vlock*);
extern void vlock_deinit(struct vlock*);

/*
 * vcondition
 */
struct vcond {
    pthread_cond_t  cond;
    pthread_mutex_t mutex;
};

extern int  vcond_init  (struct vcond*);
extern int  vcond_wait  (struct vcond*, struct vlock*);
extern int  vcond_signal(struct vcond*);
extern void vcond_deinit(struct vcond*);

/*
 * vthread
 */
typedef int (*vthread_entry_t)(void*);
struct vthread {
    pthread_mutex_t mutex;
    pthread_t thread;
    vthread_entry_t entry_cb;
    void* cookie;

    int started;
    int quited;
};

extern int  vthread_init  (struct vthread*, vthread_entry_t, void*);
extern int  vthread_start (struct vthread*);
extern int  vthread_join  (struct vthread*, int*);
extern void vthread_deinit(struct vthread*);

/*
 * vtimer
 */
typedef int (*vtimer_cb_t)(void*);
struct vtimer {
    timer_t id;
    vtimer_cb_t cb;
    void* cookie;
};

int  vtimer_init   (struct vtimer*, vtimer_cb_t, void*);
int  vtimer_start  (struct vtimer*, int timeout);
int  vtimer_restart(struct vtimer*, int timeout);
int  vtimer_stop   (struct vtimer*);
void vtimer_deinit (struct vtimer*);

/*
 * vsys
 */
int vsys_get_cpu_ratio(int*);
int vsys_get_mem_ratio(int*);
int vsys_get_io_ratio (int*);
int vsys_get_net_ratio(int*, int*);

/*
 * vlogger
 */
enum {
    VLOG_ERR    = LOG_ERR,
    VLOG_INFO   = LOG_INFO,
    VLOG_DEBUG  = LOG_DEBUG,
    VLOG_BUTT
};

int  vlogger_open  (const char*);
void vlogger_close (void);
int  vlogger_write (int, const char*, ...);
int  vlogger_writev(int, const char*, va_list);

#endif

