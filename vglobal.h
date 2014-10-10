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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sqlite3.h>

#include "utils/vlist.h"
#include "utils/varray.h"
#include "utils/vmem.h"
#include "utils/vsys.h"
#include "utils/vmisc.h"

#include "vdef.h"
#include "vnode.h"
#include "vnodeId.h"
#include "vrpc.h"
#include "vmsger.h"
#include "vticker.h"
#include "vdht.h"
#include "vroute.h"
#include "vhost.h"
#include "vcfg.h"

struct vcfg_ops     cfg_ops;
struct vdht_enc_ops dht_enc_ops;
struct vdht_dec_ops dht_dec_ops;

#if 1
#define malloc
#define realloc
#define strlen
#define strcpy
#define strncpy
#define snprintf
#define free
#define memset
#define memcpy
#define strcmp
#define strtol
#define time_t
#define uint32_t
#endif

#endif

