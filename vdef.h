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

#define vlogD(s) do { \
            printf("[D]"); \
            (s); \
            printf("\n");  \
        }

#define vlogI(s) do { \
            printf("[I]"); \
            (s); \
            printf("\n"); \
        }while(0)

#define vlogE(s) { \
            printf("[E]"); \
            (s); \
            vwhere(); \
            printf("\n"); \
        }

#define vlogE_cond(cond, s) do {\
            if ((cond)) { \
                printf("[E]"); \
                (s); \
                vwhere(); \
                printf("\n"); \
            } \
        }while(0)

#define vdump(s) do { \
            printf("##"); \
            (s); \
            printf("\n"); \
        }while(0)

#else
#define vassert()
#define vwhere()

#define vlogD(s)

#define vlogI(s)

#define vlogE(s) { \
            printf("[E]"); \
            (s); \
            printf("\n"); \
        }

#define vlogE_cond(cond, s) do {\
            if ((cond)) { \
                printf("[E]"); \
                (s); \
                printf("\n"); \
            } \
        }while(0)

#define vdump(s)
#endif

#define varg_decl(argv, idx, type, var)  type var = (type)((void**)argv)[idx]
#define type_decl(type, var, src)        type var = (type)src


#define elog_open           perror("[open]")
#define elog_fstat          perror("[fstat]")
#define elog_read           perror("[read]")
#define elog_socket         perror("[socket]")
#define elog_bind           perror("[bind]")
#define elog_fcntl          perror("[fcntl]")
#define elog_ioctl          perror("[ioctl]")
#define elog_setsockopt     perror("[setsockopt]")
#define elog_pselect        perror("[pselect]")
#define elog_recvfrom       perror("[recvfrom]")
#define elog_recvmsg        perror("[recvmsg]")
#define elog_sendto         perror("[sendto]")
#define elog_sendmsg        perror("[sendmsg]")
#define elog_timer_create   perror("[timer_create]")
#define elog_timer_settime  perror("[timer_settime]")
#define elog_timer_delete   perror("[timer_delete]")

#define elog_strdup         vlogE(printf("strdup"))
#define elog_strndup        vlogE(printf("strndup"))
#define elog_strtol         vlogE(printf("strtol"))
#define elog_snprintf       vlogE(printf("snprintf"))
#define elog_sscanf         vlogE(printf("sscanf"))
#define elog_malloc         vlogE(printf("malloc"))
#define elog_realloc        vlogE(printf("realloc]"))
#define elog_inet_aton      vlogE(printf("inet_aton"))
#define elog_inet_ntoa      vlogE(printf("inet_ntoa"))
#define elog_gethostname    vlogE(printf("gethostname"))
#define elog_gethostbyname  vlogE(printf("gethostbyname"))
#define elog_pthread_create vlogE(printf("pthread_create"))
#define elog_sqlite3_open   vlogE(printf("sqlite3_open"))
#define elog_sqlite3_exec   vlogE(printf("sqlite3_exec"))

#define elog_vthread_init   vlogE(printf("vthread_init"))
#define elog_vtimer_init    vlogE(printf("vtimer_init"))
#define elog_vtimer_stop    vlogE(printf("vtimer_stop"))
#define elog_vtimer_start   vlogE(printf("vtimer_start"))
#define elog_vtimer_restart vlogE(printf("vtimer_restart"))
#define elog_vmem_aux_alloc vlogE(printf("vmem_aux_alloc"))

#define elog_vhostaddr_get_first    vlogE(printf("vhostaddr_get_first"))
#define elog_vhostaddr_get_next     vlogE(printf("vhostaddr_get_next"))
#define elog_vsockaddr_convert      vlogE(printf("vsockaddr_convert"))
#define elog_vsockaddr_unconvert    vlogE(printf("vsockaddr_unconvert"))
#define elog_vmacaddr_get           vlogE(printf("vmacaddr_get"))
#define elog_vservice_alloc         vlogE(printf("vservice_alloc"))
#define elog_vsrvcInfo_alloc        vlogE(printf("vsrvcInfo_alloc"))
#define elog_vnodeInfo_alloc        vlogE(printf("vnodeInfo_alloc"))
#define elog_vstunc_req_alloc       vlogE(printf("vstunc_req_alloc"))
#define elog_vstunc_srv_alloc       vlogE(printf("vstunc_srv_alloc"))

#define elog_vmsg_cb_alloc          vlogE(printf("vmsg_cb_alloc"))
#define elog_vmsg_sys_alloc         vlogE(printf("vmsg_sys_alloc"))
#define elog_vpeer_alloc            vlogE(printf("vpeer_alloc"))
#define elog_vtick_cb_alloc         vlogE(printf("vtick_cb_alloc"))
#define elog_vrecord_alloc          vlogE(printf("vrecord_alloc"))

#define vlog_vmsger_init            vlogE(printf("vmsger_init"))
#define vlog_vrpc_init              vlogE(printf("vrpc_init"))

#define elog_be_alloc               vlogE(printf("be_alloc"))
#define elog_be_decode              vlogE(printf("be_decode"))

#define elog_upnpDiscover           vlogE(printf("upnpDiscover"))
#define elog_UPNP_GetValidIGD       vlogE(printf("UPNP_GetValidIGD"))
#define elog_UPNP_AddPortMapping    vlogE(printf("UPNP_AddPortMapping"))
#define elog_UPNP_GetSpecificPortMappingEntry \
                                    vlogE(printf("UPNP_GetSpecificPortMappingEntry"))
#define elog_UPNP_GetExternalIPAddress \
                                    vlogE(printf("UPNP_GetExternalIPAddress"))
#define elog_UPNP_DeletePortMapping vlogE(printf("UPNP_DeletePortMapping"))

#endif

