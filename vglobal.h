#ifndef __VGLOBAL_H__
#define __VGLOBAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <syslog.h>
#include <stdarg.h>

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
#include <netinet/if_ether.h>
#include <pthread.h>

#include "utils/vlist.h"
#include "utils/varray.h"
#include "utils/vdict.h"
#include "utils/vmem.h"
#include "utils/vsys.h"
#include "utils/vmisc.h"

#include "vdef.h"
#include "vcfg.h"
#include "vlog.h"
#include "vapp.h"
#include "vdht.h"
#include "vdht_core.h"
#include "vdhtapp.h"
#include "vrpc.h"
#include "vhost.h"
#include "vnode.h"
#include "vperf.h"
#include "vmsger.h"
#include "vroute.h"
#include "vlsctl.h"
#include "vupnpc.h"
#include "vnodeId.h"
#include "vticker.h"

#if 0
#define timer_t
#define time_t
#define uint8_t
#define uint16_t
#define uint32_t
#define int16_t
#define int32_t
#define sockaddr_in
#define sockaddr_un
#define sockaddr

#define isdigit
#define perror
#define printf
#define malloc
#define realloc
#define free
#define strlen
#define strcpy
#define strncpy
#define sprintf
#define snprintf
#define sscanf
#define strcmp
#define strncmp
#define strtol
#define strchr
#define strcat
#define strncat
#define strdup
#define strndup
#define memset
#define memcpy
#define memcmp
#define socket
#define bind
#define close
#define fcntl
#define ioctl
#define setsockopt
#define sendto
#define sendmsg
#define recvfrom
#define recvmsg
#define gethostname
#define gethostbyname
#define pselect
#define pthread_sigmask
#define time
#define ctime
#define srand
#define rand
#define sleep
#define unlink
#define getopt_long
#define optarg
#define optind
#define exit
#define open
#define write
#define stat
#define fstat
#define read
#define getpid
#define vsyslog
#define openlog
#define closelog

#define inet_aton
#define inet_ntoa
#define ntohs
#define htons
#define ntohl
#define htonl
#define errno

#define sqlite3
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
#define pthread_join
#define pthread_self

#define timer_create
#define timer_settime
#define timer_delete

#endif

extern struct vdht_enc_ops dht_enc_ops;
extern struct vdht_dec_ops dht_dec_ops;

static
inline uint8_t get_uint8(void* addr)
{
    return *(uint8_t*)addr;
}

static
inline void set_uint8(void* addr, uint8_t val)
{
    *(uint8_t*)addr = val;
    return ;
}

static
inline int16_t get_int16(void* addr)
{
    return *(int16_t*)addr;
}

static
inline void set_int16(void* addr, int16_t val)
{
    *(int16_t*)addr = val;
    return ;
}

static
inline uint16_t get_uint16(void* addr)
{
    return *(uint16_t*)addr;
}

static
inline void set_uint16(void* addr, uint16_t val)
{
    *(uint16_t*)addr = val;
    return ;
}

static
inline int32_t get_int32(void* addr)
{
    return *(int32_t*)addr;
}

static
inline void set_int32(void* addr, int32_t val)
{
    *(int32_t*)addr = val;
    return ;
};

static
inline uint32_t get_uint32(void* addr)
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

