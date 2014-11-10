#ifndef __VGLOBAL_H__
#define __VGLOBAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sqlite3.h>

#include "utils/vlist.h"
#include "utils/varray.h"
#include "utils/vmem.h"
#include "utils/vsys.h"
#include "utils/vmisc.h"

#include "vdef.h"
#include "vcfg.h"
#include "vdht.h"
#include "vrpc.h"
#include "vhost.h"
#include "vnode.h"
#include "vmsger.h"
#include "vroute.h"
#include "vlsctl.h"
#include "vnodeId.h"
#include "vticker.h"
#include "vplugin.h"

#if 0
#define timer_t
#define time_t
#define uint32_t
#define sockaddr_in
#define sockaddr_un
#define sockaddr

#define malloc
#define realloc
#define free
#define strlen
#define strcpy
#define strncpy
#define snprintf
#define strcmp
#define strtol
#define strcat
#define strncat
#define memset
#define memcpy
#define memcmp
#define socket
#define bind
#define close
#define fcntl
#define setsockopt
#define sendto
#define recvfrom
#define gethostname
#define gethostbyname
#define pselect
#define pthread_sigmask
#define time
#define srand
#define rand
#define sleep
#define unlink
#define getopt_long
#define optarg
#define optind
#define exit
#define open
#define fstat
#define read

#define inet_aton
#define inet_ntoa
#define ntohs
#define htons
#define errno

#define sqlite3_open
#define sqlite3_exec
#define sqlite3_close

#define pthread_mutex_init
#define pthread_mutex_lock
#define pthread_mutex_unlock
#define pthread_mutex_destroy
#define pthread_mutexattr_init
#define pthread_mutexattr_settype
#define pthread_mutexattr_destroy
#define pthread_cond_init
#define pthread_cond_wait
#define pthread_cond_signal
#define pthread_cond_destroy
#define pthread_create
#define pthread_destroy

#define timer_create
#define timer_settime
#define timer_delete

#endif

extern struct vdht_enc_ops dht_enc_ops;
extern struct vdht_dec_ops dht_dec_ops;

static
inline long get_int32(void* addr)
{
    return *(long*)addr;
}

static
inline void set_int32(void* addr, long val)
{
    *(long*)addr = val;
    return ;
};

static
inline unsigned long get_uint32(void* addr)
{
    return *(uint32_t*)addr;
}

static
inline void set_uint32(void* addr, uint32_t val)
{
    *(uint32_t*)addr = val;
    return ;
}

static
inline char* offset_addr(char* addr, int bytes)
{
    return addr + bytes;
}

static
inline char* unoff_addr(char* addr, int bytes)
{
    return addr - bytes;
}

#endif

