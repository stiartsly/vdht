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
#include <getopt.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "vlsctlc.h"
#include "vhashgen.h"

enum {
    VDHT_PING,
    VDHT_PING_R,
    VDHT_FIND_NODE,
    VDHT_FIND_NODE_R,
    VDHT_FIND_CLOSEST_NODES,
    VDHT_FIND_CLOSEST_NODES_R,
    VDHT_REFLEX,
    VDHT_REFLEX_R,
    VDHT_PROBE,
    VDHT_PROBE_R,
    VDHT_POST_SERVICE,
    VDHT_FIND_SERVICE,
    VDHT_FIND_SERVICE_R,
    VDHT_UNKNOWN
};

static
struct option long_options[] = {
    {"unix-domain-file",    required_argument, 0,        'U'},
    {"lsctl-socket-file",   required_argument, 0,        'S'},
    {"host-online",         no_argument,       0,        'd'},
    {"host-offline",        no_argument,       0,        'D'},
    {"host-exit",           no_argument,       0,        'x'},
    {"dump-host",           no_argument,       0,        's'},
    {"dump-config",         no_argument,       0,        'c'},
    {"add-node",            no_argument,       0,        'a'},
    {"relay-service-up",    no_argument,       0,        'r'},
    {"relay-service-down",  no_argument,       0,        'R'},
    {"relay-service-find",  no_argument,       0,        'f'},
    {"relay-service-probe", no_argument,       0,        'p'},
    {"ping",                no_argument,       0,        't'},
    {"addr",                required_argument, 0,        'm'},
    {"version",             no_argument,       0,        'v'},
    {"help",                no_argument,       0,        'h'},
    {0, 0, 0, 0}
};
static
void show_usage(void)
{
    printf("Usage: vlsctlc [OPTION...]\n");
    printf("  --unix-domain-file=UNIX_DOMAIN_FILE       unix domain path for communicating with vdhtd\n");
    printf("  --lsctl-socket-file=LSCTL_FILE            unix domain path for vdhtd to communicate\n");
    printf("\n");
    printf(" About vdhtd host options:\n");
    printf("  -d, --host-online                         request to make vdhtd host online\n");
    printf("  -D, --host-offline                        request to make vdhtd host offline\n");
    printf("  -x, --host-exit                           request to stop running for vdhtd\n");
    printf("  -s, --dump-host                           request to dump host information\n");
    printf("  -c, --dump-config                         request to dump config information\n");
    printf("\n");
    printf(" About other dht nodes options:\n");
    printf("  -a, --add-node           --addr=IP:PORT   request to join node\n");
    printf("\n");
    printf(" About service options:\n");
    printf("  -r, --relay-service-up   --addr=IP:PORT   request to post a relay service\n");
    printf("  -R, --relay-service-down --addr=IP:PORT   request to unpost a relay service\n");
    printf("  -f, --relay_service_find                  request to find relay service in vdhtd\n");
    printf("  -p, --relay-service-probe                 request to probe relay service in neighbor nodes\n");
    printf("\n");
    printf(" About bogus query options:\n");
    printf("  -t, --ping               --addr=IP:PORT   request to send ping query\n");
    printf("\n");
    printf(" Help Options:\n");
    printf("  -h, --help                                show this help message\n");
    printf("  -v, --version                             Print version\n");
    printf("\n");
}

static
void show_version(void)
{
    printf("Version: 0.0.1\n");
    return ;
}

static int has_hlp_opt = 0;
static int has_ver_opt = 0;
static
int show_help_param(int opt)
{
    has_hlp_opt = 1;
    return 0;
}
static
int show_ver_param(int opt)
{
    has_ver_opt = 1;
    return 0;
}

static char* lsctlc_socket_def = "/var/run/vdht/lsctl_client";
static char  lsctlc_socket[256];
static int   lsctlc_socket_opt = 0;
static
int lsctlc_socket_param(int opt)
{
    if (strlen(optarg) + 1 >= 256) {
        printf("Too long for option\n");
        return -1;
    }
    memset(lsctlc_socket, 0, 256);
    strcpy(lsctlc_socket, optarg);
    lsctlc_socket_opt = 1;
    return 0;
}


static char* lsctls_socket_def = "/var/run/vdht/lsctl_socket";
static char  lsctls_socket[256];
static int   lsctls_socket_opt = 0;
static
int lsctls_socket_param(int opt)
{
    if (strlen(optarg) + 1 >= 256) {
        printf("Too long for option\n");
        return -1;
    }

    memset(lsctls_socket, 0, 256);
    strcpy(lsctls_socket, optarg);
    lsctls_socket_opt = 1;
    return 0;
}

static int host_up_opt   = 0;
static int host_down_opt = 0;
static int host_exit_opt = 0;
static int host_dump_opt = 0;
static int cfg_dump_opt  = 0;
static
int host_cmds_param(int opt)
{
    switch(opt) {
    case 'd':
        host_up_opt = 1;
        break;
    case 'D':
        host_down_opt = 1;
        break;
    case 'x':
        host_exit_opt = 1;
        break;
    case 's':
        host_dump_opt = 1;
        break;
    case 'c':
        cfg_dump_opt = 1;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

static
int host_cmds_bind(struct vlsctlc* lsctlc)
{
    int ret = 0;

    if (host_up_opt) {
        ret = lsctlc->bind_ops->bind_host_up(lsctlc);
        if (ret < 0) return -1;
    }
    if (host_down_opt) {
        ret = lsctlc->bind_ops->bind_host_down(lsctlc);
        if (ret < 0) return -1;
    }
    if (host_exit_opt) {
        ret = lsctlc->bind_ops->bind_host_exit(lsctlc);
        if (ret < 0) return -1;
    }
    if (host_dump_opt) {
        ret = lsctlc->bind_ops->bind_host_dump(lsctlc);
        if (ret < 0) return -1;
    }
    if (cfg_dump_opt) {
        ret = lsctlc->bind_ops->bind_cfg_dump(lsctlc);
        if (ret < 0) return -1;
    }
    return 0;
}

static int  aux_addr_opt  = 0;
static int  aux_addr_port = 0;
static char aux_addr_ip[64];
static
int aux_address_param(int opt)
{
    char* port_addr = NULL;

    if (strlen(optarg) + 1 >= 64) {
        printf("Invalid IP\n");
        return -1;
    }

    port_addr = strchr(optarg, ':');
    if (!port_addr) {
        printf("Invalid IP\n");
        return -1;
    }
    if ((port_addr - optarg + 1) >= 64) {
        printf("Invalid IP\n");
        return -1;
    }
    memset(aux_addr_ip, 0, 64);
    strncpy(aux_addr_ip, optarg, (int)(port_addr - optarg));
    port_addr += 1;

    errno = 0;
    aux_addr_port = strtol(port_addr, NULL, 10);
    if (errno) {
        printf("Invalid IP\n");
        return -1;
    }
    aux_addr_opt = 1;
    return 0;
}

static int join_node_opt = 0;
static
int join_node_cmd_param(int opt)
{
    switch(opt) {
    case 'a':
        join_node_opt = 1;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}
static
int join_node_cmd_bind(struct vlsctlc* lsctlc)
{
    struct sockaddr_in addr;
    int ret = 0;

    if (join_node_opt && aux_addr_opt) {
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(aux_addr_port);
        ret = inet_aton(aux_addr_ip, (struct in_addr*)&addr.sin_addr);
        if (ret < 0) return -1;
        ret = lsctlc->bind_ops->bind_join_node(lsctlc, &addr);
        if (ret < 0) return -1;
    }
    return 0;
}

static int relay_service_up_opt    = 0;
static int relay_service_down_opt  = 0;
static int relay_service_find_opt  = 0;
static int relay_service_probe_opt = 0;
static
int relay_service_param(int opt)
{
    switch(opt) {
    case 'r':
        relay_service_up_opt = 1;
        break;
    case 'R':
        relay_service_down_opt = 1;
        break;
    case 'f':
        relay_service_find_opt = 1;
        break;
    case 'p':
        relay_service_probe_opt = 1;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

static
int relay_service_cmd_bind(struct vlsctlc* lsctlc)
{
    struct sockaddr_in addr;
    vsrvcHash hash;
    int ret = 0;

    ret = vhashhelper_get_stun_srvcHash(&hash);
    if (ret < 0) return -1;

    if (aux_addr_opt) {
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(aux_addr_port);
        ret = inet_aton(aux_addr_ip, (struct in_addr*)&addr.sin_addr);
        if (ret < 0) return -1;
    }
    if (relay_service_up_opt && aux_addr_opt) {
        struct sockaddr_in* addrs[] = {
            &addr
        };
        ret = lsctlc->bind_ops->bind_post_service(lsctlc, &hash, addrs, 1);
        if (ret < 0) return -1;
    }
    if (relay_service_down_opt && aux_addr_opt) {
        struct sockaddr_in* addrs[] = {
            &addr
        };
        ret = lsctlc->bind_ops->bind_unpost_service(lsctlc, &hash, addrs, 1);
        if (ret < 0) return -1;
    }
    if (relay_service_find_opt) {
        ret = lsctlc->bind_ops->bind_find_service(lsctlc, &hash);
        if (ret < 0) return -1;
    }
    if (relay_service_probe_opt) {
        ret = lsctlc->bind_ops->bind_probe_service(lsctlc, &hash);
        if (ret < 0) return -1;
    }
    return 0;
}

static int bogus_ping_opt  = 0;
static
int bogus_ping_param(int opt)
{
    switch(opt) {
    case 't':
        bogus_ping_opt = 1;
        break;
    default:
        return -1;
        break;
    }
    return 0;
}

static
int bogus_ping_cmd_bind(struct vlsctlc* lsctlc)
{
    struct sockaddr_in addr;
    int ret = 0;

    if (bogus_ping_opt && aux_addr_opt) {
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(aux_addr_port);
        ret = inet_aton(aux_addr_ip, (struct in_addr*)&addr.sin_addr);
        if (ret < 0) return -1;
        ret = lsctlc->bind_ops->bind_bogus_query(lsctlc, VDHT_PING, &addr);
        if (ret < 0) return -1;
    }
    return 0;
}

struct opt_routine {
    char opt;
    int (*parse_cb)(int);
};

struct opt_routine param_routines[] = {
    {'U', lsctlc_socket_param },
    {'S', lsctls_socket_param },
    {'d', host_cmds_param      },
    {'D', host_cmds_param      },
    {'x', host_cmds_param      },
    {'s', host_cmds_param      },
    {'c', host_cmds_param      },
    {'a', join_node_cmd_param },
    {'r', relay_service_param },
    {'R', relay_service_param },
    {'p', relay_service_param },
    {'t', bogus_ping_param    },
    {'m', aux_address_param   },
    {'v', show_ver_param      },
    {'h', show_help_param     },
    {0, 0}
};

int (*bind_routines[])(struct vlsctlc*) = {
    host_cmds_bind,
    join_node_cmd_bind,
    relay_service_cmd_bind,
    bogus_ping_cmd_bind,
    NULL
};

int main(int argc, char** argv)
{
    int opt_idx = 0;
    int ret = 0;
    int i = 0;
    int c = 0;

    if (argc <= 1) {
        printf("Few arguments\n");
        show_usage();
        exit(-1);
    }

    while(c >= 0) {
        c = getopt_long(argc, argv, "U:S:dDxscarRfptm:vh", long_options, &opt_idx);
        if (c < 0) {
            break;
        } else if (c == 0) {
            if (long_options[opt_idx].flag != 0) {
                break;
            }
            printf ("option %s", long_options[opt_idx].name);
            if (optarg) {
                printf (" with arg %s", optarg);
            }
            printf ("\n");
            continue;
        }
        else {
            struct opt_routine* routine = param_routines;
            for (i = 0; routine[i].parse_cb; i++) {
                if (routine[i].opt != c) {
                    continue;
                }
                ret = routine[i].parse_cb(c);
                if (ret < 0) {
                    exit(-1);
                }
                break;
            }
            if (!routine[i].parse_cb) {
                exit(-1);
            }
        }
     }

     if (optind < argc) {
            printf("Too many arguments.\n");
            show_usage();
            exit(-1);
     }
     if (has_hlp_opt) {
        show_usage();
        return 0;
     }
     if (has_ver_opt) {
        show_version();
        return 0;
     }
     {
        int (**bind_cb)(struct vlsctlc*) = bind_routines;
        struct vlsctlc lsctlc;
        char data[1024];
        int ret = 0;
        int tsz = 0;

        vlsctlc_init(&lsctlc);
        for (i = 0; bind_cb[i]; i++) {
            ret = bind_cb[i](&lsctlc);
            if (ret < 0) {
                printf("error: conflict arguments.\n");
                exit(-1);
            }
        }

        memset(data, 0, 1024);
        ret = lsctlc.ops->pack_cmd(&lsctlc, data, 1024);
        if (ret < 0) {
            printf("error: package wrong.\n");
            exit(-1);
        }
        tsz = ret;

        struct sockaddr_un unix_addr;
        struct sockaddr_un dest_addr;
        int fd = 0;

        if (!lsctlc_socket_opt) {
            strcpy(lsctlc_socket, lsctlc_socket_def);
        }
        if (!lsctls_socket_opt) {
            strcpy(lsctls_socket, lsctls_socket_def);
        }

        fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (fd < 0) {
            perror("socket:");
            exit(-1);
        }
        unlink(lsctlc_socket);

        unix_addr.sun_family = AF_UNIX;
        strcpy(unix_addr.sun_path, lsctlc_socket);
        ret = bind(fd, (struct sockaddr*)&unix_addr, sizeof(unix_addr));
        if (ret < 0) {
            perror("bind:");
            close(fd);
            exit(-1);
        }
        dest_addr.sun_family = AF_UNIX;
        strcpy(dest_addr.sun_path, lsctls_socket);

        ret = sendto(fd, data, tsz, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        if (ret < 0) {
            perror("sendto:");
            close(fd);
            exit(-1);
        }
        close(fd);
    }
    return 0;
}

