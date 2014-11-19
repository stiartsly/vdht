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

enum {
    VLSCTL_HOST_UP,
    VLSCTL_HOST_DOWN,
    VLSCTL_HOST_EXIT,
    VLSCTL_HOST_DUMP,
    VLSCTL_DHT_QUERY,

    VLSCTL_NODE_JOIN,
    VLSCTL_NODE_DROP,

    VLSCTL_PLUG,
    VLSCTL_UNPLUG,
    VLSCTL_PLUGIN_REQ,

    VLSCTL_CFG_DUMP,
    VLSCTL_BUTT
};

enum {
    PLUGIN_RELAY,
    PLUGIN_STUN,
    PLUGIN_VPN,
    PLUGIN_DDNS,
    PLUGIN_MROUTE,
    PLUGIN_DHASH,
    PLUGIN_APP,
    PLUGIN_BUTT
};

enum {
    VDHT_PING,
    VDHT_PING_R,
    VDHT_FIND_NODE,
    VDHT_FIND_NODE_R,
    VDHT_FIND_CLOSEST_NODES,
    VDHT_FIND_CLOSEST_NODES_R,
    VDHT_POST_SERVICE,
    VDHT_POST_SERVICE_R,
    VDHT_POST_HASH,
    VDHT_POST_HASH_R,
    VDHT_GET_PEERS,
    VDHT_GET_PEERS_R,
    VDHT_GET_PLUGIN,
    VDHT_GET_PLUGIN_R,
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
    {"del-node",            no_argument,       0,        'e'},
    {"relay",               no_argument,       0,        'R'},
    {"stun",                no_argument,       0,        'T'},
    {"vpn",                 no_argument,       0,        'P'},
    {"up",                  no_argument,       0,        'u'},
    {"down",                no_argument,       0,        'w'},
    {"addr",                required_argument, 0,        'm'},
    {"ping",                no_argument,       0,        'J'},
    {"find_node",           no_argument,       0,        'K'},
    {"find_closest_nodes",  no_argument,       0,        'L'},
    {"post_hash",           no_argument,       0,        'M'},
    {"get_peers",           no_argument,       0,        'N'},
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
    printf("  -e, --del-node           --addr=IP:PORT   request to drop node\n");
    printf("\n");
    printf(" About plugin options:\n");
    printf("  -p  --relay    --up      --addr=IP:PORT   request to post a relay service\n");
    printf("  -p  --relay    --down    --addr=IP:PORT   request to nulify a relay service\n");
    printf("  -p  --stun     --up      --addr=IP:PORT   request to post a stun  service\n");
    printf("  -p  --stun     --down    --addr=IP:PORT   request to nulify a stun service\n");
    printf("  -p  --vpn      --up      --addr=IP:PORT   request to post a vpn service\n");
    printf("  -p  --vpn      --down    --addr=IP:PORT   request to nulify a vpn service\n");
    printf("  -q  --relay                               request to acquire relay service\n");
    printf("  -q  --stun                                request to acquire stun service\n");
    printf("  -q  --vpn                                 request to actuire vpn service\n");
    printf("\n");
    printf(" About bogus dht options:\n");
    printf("  -t  --ping               --addr=IP:PORT   request to send ping query\n");
    printf("  -t  --find_node          --addr=IP:PORT   request to send find_node query\n");
    printf("  -t  --find_closest_nodes --addr=IP:PORT   reqeust to send find_closest_nodes query\n");
    printf("  -t  --post_hash          --addr=IP:PORT   request to send post_hash query\n");
    printf("  -t  --get_peers          --addr=IP:PORT   request to send get_peers query\n");
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

static int has_help_param = 0;
static int has_ver_param  = 0;
static int show_help_param(int opt)
{
    has_help_param = 1;
    return 0;
}
static int show_ver_param(int opt)
{
    has_ver_param = 1;
    return 0;
}

static char* lsctlc_socket_def = "/var/run/vdht/lsctl_client";
static char  lsctlc_socket[256];
static int   lsctlc_socket_sym = 0;
static int   lsctlc_socket_param(int opt)
{
    if (strlen(optarg) + 1 >= 256) {
        printf("Too long for option\n");
        return -1;
    }
    memset(lsctlc_socket, 0, 256);
    strcpy(lsctlc_socket, optarg);
    lsctlc_socket_sym = 1;
    return 0;
}
static int   lsctlc_socket_check(void)
{
    if (!lsctlc_socket_sym) {
        strcpy(lsctlc_socket, lsctlc_socket_def);
        lsctlc_socket_sym = 1;
    }
    return 0;
}

static char* lsctls_socket_def = "/var/run/vdht/lsctl_socket";
static char  lsctls_socket[256];
static int   lsctls_socket_sym = 0;
static int   lsctls_socket_param(int opt)
{
    if (strlen(optarg) + 1 >= 256) {
        printf("Too long for option\n");
        return -1;
    }

    memset(lsctls_socket, 0, 256);
    strcpy(lsctls_socket, optarg);
    lsctls_socket_sym = 1;
    return 0;
}
static int   lsctls_socket_check(void)
{
    if (!lsctls_socket_sym) {
        strcpy(lsctls_socket, lsctls_socket_def);
        lsctls_socket_sym = 1;
    }
    return 0;
}
static int has_host_online_param  = 0;
static int has_host_offline_param = 0;
static int has_host_exit_param    = 0;
static int has_host_dump_param    = 0;
static int has_cfg_dump_param     = 0;
static int host_sort_cmd_param(int opt)
{
    switch(opt) {
    case 'd':
        has_host_online_param = 1;
        break;
    case 'D':
        has_host_offline_param = 1;
        break;
    case 'x':
        has_host_exit_param = 1;
        break;
    case 's':
        has_host_dump_param = 1;
        break;
    case 'c':
        has_cfg_dump_param = 1;
        break;
    default:
        break;
    }
    return 0;
}
static int has_host_online_cmd  = 0;
static int has_host_offline_cmd = 0;
static int has_host_exit_cmd    = 0;
static int has_host_dump_cmd    = 0;
static int has_cfg_dump_cmd     = 0;

static int host_sort_cmd_check(void)
{
    if (has_host_online_param) {
        if (has_host_offline_param || has_host_exit_param) {
            printf("Conflict options\n");
            return -1;
        }
        has_host_online_cmd = 1;
    }
    if (has_host_offline_param) {
        if (has_host_online_param || has_host_exit_param) {
            printf("Conflict options\n");
            return -1;
        }
        has_host_offline_cmd = 1;
    }
    if (has_host_exit_param) {
        if (has_host_online_param ||
            has_host_offline_param ||
            has_host_dump_param ||
            has_cfg_dump_param) {
            printf("Confilict options\n");
            return -1;
        }
        has_host_exit_cmd = 1;
    }
    if (has_host_dump_param) {
        if (has_host_exit_param) {
            printf("Conflict options\n");
            return -1;
        }
        has_host_dump_cmd = 1;
    }
    if (has_cfg_dump_param) {
        if (has_host_exit_param) {
            printf("Conflict options\n");
            return -1;
        }
        has_cfg_dump_cmd = 1;
    }
    return 0;
}

static int host_sort_cmd_pack(char* data)
{
    int sz = 0;
    if (has_host_online_cmd) {
        *(int32_t*)data = VLSCTL_HOST_UP;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    }
    if (has_host_offline_cmd) {
        *(int32_t*)data = VLSCTL_HOST_DOWN;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    }
    if (has_host_exit_cmd) {
        *(int32_t*)data = VLSCTL_HOST_EXIT;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    }
    if (has_host_dump_cmd) {
        *(int32_t*)data = VLSCTL_HOST_DUMP;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    }
    if (has_cfg_dump_cmd) {
        *(int32_t*)data = VLSCTL_CFG_DUMP;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    }
    return sz;
}

static int has_plugin_set_param   = 0;
static int has_plugin_req_param   = 0;
static int has_plugin_relay_param = 0;
static int has_plugin_stun_param  = 0;
static int has_plugin_vpn_param   = 0;
static int has_plugin_up_param    = 0;
static int has_plugin_down_param  = 0;
static int plgn_sort_cmd_param(int opt)
{
    switch(opt) {
    case 'p':
        has_plugin_set_param   = 1;
        break;
    case 'q':
        has_plugin_req_param   = 1;
        break;
    case 'R':
        has_plugin_relay_param = 1;
        break;
    case 'T':
        has_plugin_stun_param  = 1;
        break;
    case 'P':
        has_plugin_vpn_param   = 1;
        break;
    case 'u':
        has_plugin_up_param    = 1;
        break;
    case 'w':
        has_plugin_down_param  = 1;
        break;
    default:
        break;
    }
    return 0;
}
static int has_addr_param = 0;
static char addr_ip[64];
static char addr_port = 0;
static int addr_opt_param(int opt)
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
    strncpy(addr_ip, optarg, (int)(port_addr - optarg));
    port_addr += 1;

    errno = 0;
    addr_port = strtol(port_addr, NULL, 10);
    if (errno) {
        printf("Invalid IP\n");
        return -1;
    }
    has_addr_param = 1;
    return 0;
}

static int has_plugin_set_cmd = 0;
static int has_plugin_req_cmd = 0;
static int plgn_sort_cmd_check(void)
{
    if (has_plugin_set_param && has_plugin_req_param) {
        printf("Confilict options for '-p' and '-q'\n");
        return -1;
    }
    if (has_plugin_set_param) {
        if ((!has_plugin_relay_param) &&
            (!has_plugin_stun_param)  &&
            (!has_plugin_vpn_param)) {
            printf("Few options\n");
            return -1;
        }
        if (has_plugin_relay_param && has_plugin_stun_param) {
            printf("Conflict options for '--relay' and '--stun'\n");
            return -1;
        }
        if (has_plugin_relay_param && has_plugin_vpn_param) {
            printf("Conflict options for '--relay' and '--vpn'\n");
            return -1;
        }
        if (has_plugin_stun_param && has_plugin_vpn_param) {
            printf("Confilict options for '--stun' and '--vpn'\n");
            return -1;
        }
        if ((!has_plugin_up_param) && (!has_plugin_down_param)) {
            printf("Few options\n");
            return -1;
        }
        if (has_plugin_up_param && has_plugin_down_param) {
            printf("Conflict options for '--up' and '--down'\n");
            return -1;
        }
        if (!has_addr_param) {
            printf("Few options\n");
            return -1;
        }
        has_plugin_set_cmd = 1;
        return 0;
    }
    if (has_plugin_req_param) {
        if ((!has_plugin_relay_param) &&
            (!has_plugin_stun_param)  &&
            (!has_plugin_vpn_param)) {
            printf("Few options\n");
            return -1;
        }
        if (has_plugin_relay_param && has_plugin_stun_param) {
            printf("Conflict options for '--relay' and '--stun'\n");
            return -1;
        }
        if (has_plugin_relay_param && has_plugin_vpn_param) {
            printf("Conflict options for '--relay' and '--vpn'\n");
            return -1;
        }
        if (has_plugin_stun_param && has_plugin_vpn_param) {
            printf("Confilict options for '--stun' and '--vpn'\n");
            return -1;
        }
        if (has_addr_param) {
            printf("Redundant argument '--addr'\n");
            return -1;
        }
        has_plugin_req_cmd = 1;
        return 0;
    }
    if ((!has_plugin_set_param) && (!has_plugin_req_param)) {
        if (has_plugin_relay_param ||
            has_plugin_stun_param  ||
            has_plugin_vpn_param) {
            printf("Redundant argument\n");
            return -1;
        }
        if (has_plugin_up_param || has_plugin_down_param) {
            printf("Redundant argument\n");
            return -1;
        }
    }
    return 0;
}

static int plgn_sort_cmd_pack(char* data)
{
    int sz = 0;
    if (has_plugin_set_cmd) {
        if (has_plugin_up_param) {
            *(int32_t*)data = VLSCTL_PLUG;
        } else if (has_plugin_down_param) {
            *(int32_t*)data = VLSCTL_UNPLUG;
        } else {
            return -1;
        }
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    } else if (has_plugin_req_cmd) {
        *(int32_t*)data = VLSCTL_PLUGIN_REQ;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
    } else {
        return -1;
    }

    if (has_plugin_relay_param) {
        *(int32_t*)data = PLUGIN_RELAY;
    } else if (has_plugin_stun_param) {
        *(int32_t*)data = PLUGIN_STUN;
    } else if (has_plugin_vpn_param) {
        *(int32_t*)data = PLUGIN_VPN;
    } else {
        return -1;
    }
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    if (has_addr_param) {
        *(int32_t*)data = addr_port;
        data += sizeof(int32_t);
        sz   += sizeof(int32_t);
        strcpy(data, addr_ip);
        data += strlen(addr_ip) + 1;
        sz   += strlen(addr_ip) + 1;
    }
    return sz;
}

static int has_join_node_param = 0;
static int has_drop_node_param = 0;
static int node_sort_cmd_param(int opt)
{
    switch(opt) {
    case 'a':
        has_join_node_param = 1;
        break;
    case 'e':
        has_drop_node_param = 1;
        break;
    default:
        return -1;
    }
    return 0;
}
static int has_join_node_cmd = 0;
static int has_drop_node_cmd = 0;
static int node_sort_cmd_check(void)
{
    if (has_join_node_param) {
        if (!has_addr_param) {
            printf("Few arguments\n");
            return -1;
        }
        has_join_node_cmd = 1;
    }
    if (has_drop_node_param) {
        if (!has_addr_param) {
            printf("Few arguments\n");
            return -1;
        }
        has_drop_node_cmd = 1;
    }
    return 0;
}

static int node_sort_cmd_pack(char* data)
{
    int sz = 0;

    if (has_join_node_cmd) {
        *(int32_t*)data = VLSCTL_NODE_JOIN;
    } else if (has_drop_node_cmd) {
        *(int32_t*)data = VLSCTL_NODE_DROP;
    } else {
        return -1;
    }
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    *(int32_t*)data = addr_port;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    strcpy(data, addr_ip);
    data += strlen(addr_ip) + 1;
    sz   += strlen(addr_ip) + 1;
    return sz;
}

static int has_dht_query_param = 0;
static int has_dht_ping_param  = 0;
static int has_dht_find_node_param = 0;
static int has_dht_find_closest_nodes_param = 0;
static int has_dht_post_hash_param = 0;
static int has_dht_get_peers_param = 0;
static int dht_sort_cmd_param(int opt)
{
    switch(opt) {
    case 't':
        has_dht_query_param = 1;
        break;
    case 'J':
        has_dht_ping_param = 1;
        break;
    case 'K':
        has_dht_find_node_param =1;
        break;
    case 'L':
        has_dht_find_closest_nodes_param = 1;
        break;
    case 'M':
        has_dht_post_hash_param = 1;
        break;
    case 'N':
        has_dht_get_peers_param = 1;
        break;
    default:
        return -1;
    }
    return 0;
}

static int has_dht_query_cmd = 0;
static int dht_sort_cmd_check(void)
{
    if (!has_dht_query_param) {
        if (has_dht_ping_param ||
            has_dht_find_node_param ||
            has_dht_find_closest_nodes_param ||
            has_dht_post_hash_param ||
            has_dht_get_peers_param) {
            printf("Redundant argument\n");
            return -1;
        }
    } else {
        if ((!has_dht_ping_param) &&
            (!has_dht_find_node_param) &&
            (!has_dht_find_closest_nodes_param) &&
            (!has_dht_post_hash_param) &&
            (!has_dht_get_peers_param)) {
            printf("Few arguments\n");
            return -1;
        }
        if ((has_dht_ping_param && has_dht_find_node_param) ||
            (has_dht_ping_param && has_dht_find_closest_nodes_param) ||
            (has_dht_ping_param && has_dht_post_hash_param) ||
            (has_dht_ping_param && has_dht_get_peers_param) ||
            (has_dht_find_node_param && has_dht_find_closest_nodes_param) ||
            (has_dht_find_node_param && has_dht_post_hash_param) ||
            (has_dht_find_node_param && has_dht_get_peers_param) ||
            (has_dht_find_closest_nodes_param && has_dht_post_hash_param) ||
            (has_dht_find_closest_nodes_param && has_dht_get_peers_param) ||
            (has_dht_post_hash_param && has_dht_get_peers_param)) {
            printf("Redundant arguments\n");
            return -1;
        }
        if (!has_addr_param) {
            printf("Few arguments\n");
            return -1;
        }
        has_dht_query_cmd = 1;
    }
    return 0;
}

static int dht_sort_cmd_pack(char* data)
{
    int sz = 0;
    if (!has_dht_query_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_DHT_QUERY;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    if (has_dht_ping_param) {
        *(int32_t*)data = VDHT_PING;
    }else if (has_dht_find_node_param) {
        *(int32_t*)data = VDHT_FIND_NODE;
    }else if (has_dht_find_closest_nodes_param) {
        *(int32_t*)data = VDHT_FIND_CLOSEST_NODES;
    }else if (has_dht_post_hash_param) {
        *(int32_t*)data = VDHT_POST_HASH;
    }else if (has_dht_get_peers_param) {
        *(int32_t*)data = VDHT_GET_PEERS;
    }else {
        return -1;
    }
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    *(int32_t*)data = addr_port;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    strcpy(data, addr_ip);
    data += strlen(addr_ip) + 1;
    sz   += strlen(addr_ip) + 1;
    return sz;
}

static int msg_header_pack(char* data)
{
    int sz = 0;

    *(uint32_t*)data = 0x7fec45fa; //lsctl magic;
    data += sizeof(uint32_t);
    sz   += sizeof(uint32_t);

    *(int32_t*)data = 0x45; //VMSG_LSCTL;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    return sz;
}

static int msg_end_mark_pack(char* data)
{
    int sz =0;
    *(int32_t*)data = 0x7fec45fa; //remark end of msg.
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    return sz;
}

struct opt_routine {
    char opt;
    int (*parse_cb)(int);
};

struct opt_routine param_routines[] = {
    {'U', lsctlc_socket_param },
    {'S', lsctls_socket_param },
    {'d', host_sort_cmd_param },
    {'D', host_sort_cmd_param },
    {'x', host_sort_cmd_param },
    {'s', host_sort_cmd_param },
    {'c', host_sort_cmd_param },
    {'a', node_sort_cmd_param },
    {'e', node_sort_cmd_param },
    {'p', plgn_sort_cmd_param },
    {'R', plgn_sort_cmd_param },
    {'T', plgn_sort_cmd_param },
    {'P', plgn_sort_cmd_param },
    {'u', plgn_sort_cmd_param },
    {'w', plgn_sort_cmd_param },
    {'q', plgn_sort_cmd_param },
    {'m', addr_opt_param      },
    {'t', dht_sort_cmd_param  },
    {'J', dht_sort_cmd_param  },
    {'K', dht_sort_cmd_param  },
    {'L', dht_sort_cmd_param  },
    {'M', dht_sort_cmd_param  },
    {'N', dht_sort_cmd_param  },
    {'v', show_ver_param      },
    {'h', show_help_param     },
    {0, 0}
};

int (*check_routines[])(void) = {
    lsctlc_socket_check,
    lsctls_socket_check,
    host_sort_cmd_check,
    plgn_sort_cmd_check,
    node_sort_cmd_check,
    dht_sort_cmd_check,
    NULL
};

int (*pack_routines[])(char*) = {
    msg_header_pack,   // must be first of array.
    host_sort_cmd_pack,
    plgn_sort_cmd_pack,
    node_sort_cmd_pack,
    dht_sort_cmd_pack,
    msg_end_mark_pack, // must be end of array.
    NULL,
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
        c = getopt_long(argc, argv, "U:S:dDxscaepRTPuwm:qtJKLMNvh", long_options, &opt_idx);
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
     if (has_help_param) {
        show_usage();
        return 0;
     }
     if (has_ver_param) {
        show_version();
        return 0;
     }
     {
        int (**check_cb)(void) = check_routines;
        int (**pack_cb)(char*) = pack_routines;
        char  data[1024];
        char* buf = data;
        memset(data, 0, 1024);

        for (i = 0; check_cb[i]; i++) {
            ret = check_cb[i]();
            if (ret < 0) {
                exit(-1);
            }
        }
        for (i = 0; pack_cb[i] ; i++) {
            ret = pack_cb[i](buf);
            if (ret < 0) {
                continue;
            }
            buf += ret;
        }
        struct sockaddr_un unix_addr;
        struct sockaddr_un dest_addr;
        int fd = 0;

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

        ret = sendto(fd, data, (int)(buf-data), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        if (ret < 0) {
            perror("sendto:");
            close(fd);
            exit(-1);
        }
        close(fd);
    }
    return 0;
}

