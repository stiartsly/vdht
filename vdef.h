#ifndef __VDEF_H__
#define __VDEF_H__

#define vcall_cond(cond,s)         do{if(cond) {s;}}while(0)
#define vcall2_cond(cond,s1,s2)    do{if(cond) {s1;s2;}}while(0)
#define vcall3_cond(cond,s1,s2,s3) do{if(cond) {s1;s2;s3;}}while(0)

#define retE(cond)          vcall_cond ((cond),return -1)
#define ret1E(cond,s)       vcall2_cond((cond),(s), return -1)
#define ret2E(cond,s1,s2)   vcall3_cond((cond),(s1),(s2),return -1)

#define retE_p(cond)        vcall_cond ((cond),return NULL)
#define ret1E_p(cond,s)     vcall2_cond((cond),(s), return NULL)
#define ret2E_p(cond,s1,s2) vcall3_cond((cond),(s1),(s2),return NULL)

#define retE_v(cond)        vcall_cond ((cond),return)
#define ret1E_v(cond,s)     vcall2_cond((cond),(s), return)
#define ret2E_v(cond,s1,s2) vcall3_cond((cond),(s1),(s2),return)

#define retS(cond)          vcall_cond ((cond),return 0)
#define ret1S(cond,s)       vcall2_cond((cond),(s), return 0)
#define ret2S(cond,s1,s2)   vcall3_cond((cond),(s1),(s2),return 0)

/*
 *
 */
#define _DEBUG
#ifdef _DEBUG
#define vassert(cond)   do { \
            printf("{assert}[%s:%d]\n", __FUNCTION__,__LINE__); \
            *((int*)0) = 0; \
        } while(0)

#define vwhere()        do { \
            printf("{where}[%s:%d]", __FUNCTION__,__LINE__); \
        } while(0)

#define vlog_cond(cond,s) do {  \
            if (cond) {\
                s; \
                vwhere(); \
            } \
        }while(0)

#define vlog(s) { \
            printf("["); \
            (s); \
            printf("] error"); \
            vwhere(); \
        }
#else
#define vassert()
#define vwhere()
#define vlog_cond(cond,s) do {  \
            if (cond) {\
                s; \
            } \
        }while(0)

#define vlog(s) { \
            printf("["); \
            (s); \
            printf("] error"); \
        }
#endif



#define INT32(data)           (*(long*)data)
#define SET_INT32(data, val)  (*(long*)data = (long)val)
#define OFF_INT32(data)       ((long*)data + 1)
#define UNOFF_INT32(data)     ((long*)data - 1)

#define UINT32(data)          (*(uint32_t*)data)
#define SET_UINT32(data,val)  (*(uint32_t*)data = (uint32_t)val)
#define OFF_UINT32(data)      ((uint32_t*)data + 1)
#define UNOFF_UINT32(data)    ((uint32_t*)data - 1)



#define elog_socket         perror("[socket]")
#define elog_bind           perror("[bind]")
#define elog_fcntl          perror("[fcntl]")
#define elog_setsockopt     perror("[setsockopt]")
#define elog_pselect        perror("[pselect]")
#define elog_recvfrom       perror("[recvfrom]")
#define elog_sendto         perror("[sendto]")

#define elog_malloc         vlog(printf("malloc"))
#define elog_realloc        vlog(printf("realloc]"))
#define elog_inet_aton      vlog(printf("inet_aton"))
#define elog_inet_ntoa      vlog(printf("inet_ntoa"))
#define elog_vtimer_init    vlog(printf("vtimer_init"))
#define elog_vtimer_stop    vlog(printf("vtimer_stop"))
#define elog_vtimer_start   vlog(printf("vtimer_start"))
#define elog_vtimer_restart vlog(printf("vtimer_restart"))
#define elog_pthread_create vlog(printf("pthread_create"))
#define elog_vmem_aux_alloc vlog(printf("vmem_aux_alloc"))
#define elog_sqlite3_open   vlog(printf("sqlite3_open"))
#define elog_sqlite3_exec   vlog(printf("sqlite3_exec"))
#define elog_vmsg_cb_alloc  vlog(printf("vmsg_cb_alloc"))
#define elog_vmsg_sys_alloc vlog(printf("vmsg_sys_alloc"))
#define elog_vpeer_alloc    vlog(printf("vpeer_alloc"))
#define elog_vtick_cb_alloc vlog(printf("vtick_cb_alloc"))

#define elog_be_alloc       vlog(printf("be_alloc"))
#define elog_be_decode      vlog(printf("be_decode"))

#endif
