#ifndef __VDEF_H__
#define __VDEF_H__

#define BUF_SZ (1024)

#define vcall_cond(cond,s)           do{if(cond) {s;}}while(0)
#define vcall2_cond(cond,s1,s2)      do{if(cond) {s1;s2;}}while(0)
#define vcall3_cond(cond,s1,s2,s3)   do{if(cond) {s1;s2;s3;}}while(0)

#define vcall_cond_d(cond,s)         do{if(cond) {vwhere();s;}}while(0)
#define vcall2_cond_d(cond,s1,s2)    do{if(cond) {vwhere();s1;s2;}}while(0)
#define vcall3_cond_d(cond,s1,s2,s3) do{if(cond) {vwhere();s1;s2;s3;}}while(0)

#define retE(cond)          vcall_cond_d ((cond),return -1)
#define ret1E(cond,s)       vcall2_cond_d((cond),(s), return -1)
#define ret2E(cond,s1,s2)   vcall3_cond_d((cond),(s1),(s2),return -1)

#define retE_p(cond)        vcall_cond_d ((cond),return NULL)
#define ret1E_p(cond,s)     vcall2_cond_d((cond),(s), return NULL)
#define ret2E_p(cond,s1,s2) vcall3_cond_d((cond),(s1),(s2),return NULL)

#define retE_v(cond)        vcall_cond_d ((cond),return)
#define ret1E_v(cond,s)     vcall2_cond_d((cond),(s), return)
#define ret2E_v(cond,s1,s2) vcall3_cond_d((cond),(s1),(s2),return)

#define retS(cond)          vcall_cond   ((cond),return 0)
#define ret1S(cond,s)       vcall2_cond  ((cond),(s), return 0)
#define ret2S(cond,s1,s2)   vcall3_cond  ((cond),(s1),(s2),return 0)

/*
 *
 */
#define _DEBUG
#ifdef _DEBUG
#define vassert(cond) do { \
            if (!(cond)) { \
                printf("{assert}[%s:%d]\n", __FUNCTION__,__LINE__); \
                *((int*)0) = 0; \
            } \
        } while(0)

#define vwhere() do { \
            printf("$${where}[%s:%d]\n", __FUNCTION__,__LINE__); \
        } while(0)

#define vdump(s) do { \
            printf("##"); \
            (s); \
            printf("\n"); \
        }while(0)

#else
#define vassert()
#define vwhere()
#define vdump(s)
#endif

#define varg_decl(argv, idx, type, var)  type var = (type)((void**)argv)[idx]
#define type_decl(type, var, src)        type var = (type)src

#define elog_open           strerror(errno)
#define elog_fstat          strerror(errno)
#define elog_read           strerror(errno)
#define elog_socket         strerror(errno)
#define elog_bind           strerror(errno)
#define elog_fcntl          strerror(errno)
#define elog_ioctl          strerror(errno)
#define elog_setsockopt     strerror(errno)
#define elog_pselect        strerror(errno)
#define elog_recvfrom       strerror(errno)
#define elog_recvmsg        strerror(errno)
#define elog_sendto         strerror(errno)
#define elog_sendmsg        strerror(errno)
#define elog_timer_create   strerror(errno)
#define elog_timer_settime  strerror(errno)
#define elog_timer_delete   strerror(errno)

#define elog_strdup         "{strdup} error"
#define elog_strndup        "{strndup} error"
#define elog_strtol         "{strtol} error"
#define elog_snprintf       "{snprintf} error"
#define elog_sscanf         "{sscanf} error"
#define elog_malloc         "{malloc} error"
#define elog_realloc        "{realloc} error"
#define elog_inet_aton      "{inet_aton} error"
#define elog_inet_ntoa      "{inet_ntoa} error"
#define elog_gethostname    "{gethostname} error"
#define elog_gethostbyname  "{gethostbyname} error"
#define elog_pthread_create "{pthread_create} error"
#define elog_sqlite3_open   "{sqlite3_open} error"
#define elog_sqlite3_exec   "{sqlite3_exec} error"

#define elog_vthread_init   "{vthread_init} error"
#define elog_vtimer_init    "{vtimer_init} error"
#define elog_vtimer_stop    "{vtimer_stop} error"
#define elog_vtimer_start   "{vtimer_start} error"
#define elog_vtimer_restart "{vtimer_restart} error"
#define elog_vmem_aux_alloc "{vmem_aux_alloc} error"

#define elog_vhostaddr_get_first    "{vhostaddr_get_first} error"
#define elog_vhostaddr_get_next     "{vhostaddr_get_next} error"
#define elog_vsockaddr_convert      "{vsockaddr_convert} error"
#define elog_vsockaddr_unconvert    "{vsockaddr_unconvert} error"
#define elog_vmacaddr_get           "{vmacaddr_get} error"
#define elog_vservice_alloc         "{vservice_alloc} error"
#define elog_vsrvcInfo_alloc        "{vsrvcInfo_alloc} error"
#define elog_vnodeInfo_alloc        "{vnodeInfo_alloc} error"
#define elog_vstunc_req_alloc       "{vstunc_req_alloc} error"
#define elog_vstunc_srv_alloc       "{vstunc_srv_alloc} error"

#define elog_vmsg_cb_alloc          "{vmsg_cb_alloc} error"
#define elog_vmsg_sys_alloc         "{vmsg_sys_alloc} error"
#define elog_vpeer_alloc            "{vpeer_alloc} error"
#define elog_vtick_cb_alloc         "{vtick_cb_alloc} error"
#define elog_vrecord_alloc          "{vrecord_alloc} error"

#define vlog_vmsger_init            "{vmsger_init} error"
#define vlog_vrpc_init              "{vrpc_init} error"

#define elog_be_alloc               "{be_alloc} error"
#define elog_be_decode              "{be_decode} error"

#define elog_upnpDiscover           "{upnpDiscover} error"
#define elog_UPNP_GetValidIGD       "{UPNP_GetValidIGD} error"
#define elog_UPNP_AddPortMapping    "{UPNP_AddPortMapping} error"
#define elog_UPNP_GetSpecificPortMappingEntry \
                                    "{UPNP_GetSpecificPortMappingEntry} error"
#define elog_UPNP_GetExternalIPAddress \
                                    "{UPNP_GetExternalIPAddress} error"
#define elog_UPNP_DeletePortMapping "{UPNP_DeletePortMapping} error"

#endif

