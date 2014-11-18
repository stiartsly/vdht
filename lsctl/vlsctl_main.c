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
    VDHT_GET_PEERS,
    VDHT_GET_PEERS_R,
    VDHT_POST_HASH,
    VDHT_POST_HASH_R,
    VDHT_FIND_CLOSEST_NODES,
    VDHT_FIND_CLOSEST_NODES_R,
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
    printf("  -a, --add-node                            request to join node\n");
    printf("  -e, --del-node                            request to drop node\n");
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
    printf("  -H, --help                                show this help message\n");
    printf("  -V, --version                             Print version\n");
    printf("\n");
}

static char* lsctlc_socket_def = "/var/run/vdht/lsctl_client";
static char  lsctlc_socket[256];
static int   lsctlc_socket_sym = 0;
static int   lsctlc_socket_param(void)
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
static int   lsctls_socket_param(void)
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

static int host_online_opt  = 0;
static int host_offline_opt = 0;
static int host_exit_opt    = 0;
static int host_dump_opt    = 0;
static int config_dump_opt  = 0;
static int host_online_opt_parse(void)
{
    host_online_opt = 1;
    return 0;
}
static int host_offline_opt_parse(void)
{

    host_offline_opt = 1;
    return 0;
}
static int host_exit_opt_parse(void)
{
    host_exit_opt = 1;
    return 0;
}
static int host_dump_opt_parse(void)
{
    host_dump_opt = 1;
    return 0;
}
static int config_dump_opt_parse(void)
{
    config_dump_opt = 1;
    return 0;
}

static int host_online_cmd  = 0;
static int host_offline_cmd = 0;
static int host_exit_cmd    = 0;
static int host_dump_cmd    = 0;
static int config_dump_cmd  = 0;

static int host_opt_check(void)
{
    if (host_online_opt) {
        if (host_offline_opt || host_exit_opt) {
            printf("Conflict options\n");
            return -1;
        }
        host_online_cmd = 1;
    }
    if (host_offline_opt) {
        if (host_online_opt || host_exit_opt) {
            printf("Conflict options\n");
            return -1;
        }
        host_offline_cmd = 1;
    }
    if (host_exit_opt) {
        if (host_online_opt ||
            host_offline_opt ||
            host_dump_opt ||
            config_dump_opt) {
            printf("Confilict options\n");
            return -1;
        }
        host_exit_cmd = 1;
    }
    if (host_dump_opt) {
        if (host_exit_opt) {
            printf("Conflict options\n");
            return -1;
        }
        host_dump_cmd = 1;
    }
    if (config_dump_opt) {
        if (host_exit_opt) {
            printf("Conflict options\n");
            return -1;
        }
        config_dump_cmd = 1;
    }
    return 0;
}

static int host_online_cmd_pack(char* data)
{
    int sz = 0;
    if (!host_online_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_HOST_UP;
    sz += sizeof(int32_t);
    return sz;

}

static int host_offline_cmd_pack(char* data)
{
    int sz = 0;
    if (!host_offline_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_HOST_DOWN;
    sz += sizeof(int32_t);
    return sz;
}

static int host_exit_cmd_pack(char* data)
{
    int sz = 0;
    if (!host_exit_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_HOST_EXIT;
    sz += sizeof(int32_t);
    return sz;
}

static int host_dump_cmd_pack(char* data)
{
    int sz = 0;
    if (!host_dump_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_HOST_DUMP;
    sz += sizeof(int32_t);
    return sz;
}

static int config_dump_cmd_pack(char* data)
{
    int sz = 0;
    if (!config_dump_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_CFG_DUMP;
    sz += sizeof(int32_t);
    return sz;
}

static int plugin_opt = 0;
static int plugin_opt_parse(void)
{
    plugin_opt = 1;
    return 0;
}
static int plugin_relay_opt = 0;
static int plugin_relay_opt_parse(void)
{
    plugin_relay_opt = 1;
    return 0;
}
static int plugin_stun_opt = 0;
static int plugin_stun_opt_parse(void)
{
    plugin_stun_opt = 1;
    return 0;
}
static int plugin_vpn_opt = 0;
static int plugin_vpn_opt_parse(void)
{
    plugin_vpn_opt = 1;
    return 0;
}
static int plugin_up_opt = 0;
static int plugin_up_opt_parse(void)
{
    plugin_up_opt = 1;
    return 0;
}
static int plugin_down_opt = 0;
static int plugin_down_opt_parse(void)
{
    plugin_down_opt = 1;
    return 0;
}
static int addr_opt  = 0;
static char addr_ip[64];
static int addr_port = 0;
static int addr_opt_parse(void)
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
    addr_opt = 1;
    return 0;
}

static int plugin_req_opt = 0;
static int plugin_req_opt_parse(void)
{
    plugin_req_opt = 1;
    return 0;
}

static int plugin_cmd     = 0;
static int plugin_req_cmd = 0;
static int plugin_opt_check(void)
{
    if (plugin_opt && plugin_req_opt) {
        printf("Confilict options for '-p' and '-q'\n");
        return -1;
    }
    if (plugin_opt) {
        if ((!plugin_relay_opt) &&
            (!plugin_stun_opt)  &&
            (!plugin_vpn_opt)) {
            printf("Few options\n");
            return -1;
        }
        if (plugin_relay_opt && plugin_stun_opt) {
            printf("Conflict options for '--relay' and '--stun'\n");
            return -1;
        }
        if (plugin_relay_opt && plugin_vpn_opt) {
            printf("Conflict options for '--relay' and '--vpn'\n");
            return -1;
        }
        if (plugin_stun_opt && plugin_vpn_opt) {
            printf("Confilict options for '--stun' and '--vpn'\n");
            return -1;
        }
        if ((!plugin_up_opt) && (!plugin_down_opt)) {
            printf("Few options\n");
            return -1;
        }
        if (plugin_up_opt && plugin_down_opt) {
            printf("Conflict options for '--up' and '--down'\n");
            return -1;
        }
        if (!addr_opt) {
            printf("Few options\n");
            return -1;
        }
        plugin_cmd = 1;
        return 0;
    }
    if (plugin_req_opt) {
        if ((!plugin_relay_opt) &&
            (!plugin_stun_opt)  &&
            (!plugin_vpn_opt)) {
            printf("Few options\n");
            return -1;
        }
        if (plugin_relay_opt && plugin_stun_opt) {
            printf("Conflict options for '--relay' and '--stun'\n");
            return -1;
        }
        if (plugin_relay_opt && plugin_vpn_opt) {
            printf("Conflict options for '--relay' and '--vpn'\n");
            return -1;
        }
        if (plugin_stun_opt && plugin_vpn_opt) {
            printf("Confilict options for '--stun' and '--vpn'\n");
            return -1;
        }
        if (addr_opt) {
            printf("Redundant argument '--addr'\n");
            return -1;
        }
        plugin_req_cmd = 1;
        return 0;
    }
    if ((!plugin_opt) && (!plugin_req_opt)) {
        if (plugin_relay_opt ||
            plugin_stun_opt  ||
            plugin_vpn_opt) {
            printf("Redundant argument\n");
            return -1;
        }
        if (plugin_up_opt || plugin_down_opt) {
            printf("Redundant argument\n");
            return -1;
        }
    }
    return 0;
}

static int plugin_cmd_pack(char* data)
{
    int sz = 0;
    if (!plugin_cmd) {
        return -1;
    }
    if (plugin_up_opt) {
        *(int32_t*)data = VLSCTL_PLUG;
    } else if (plugin_down_opt) {
        *(int32_t*)data = VLSCTL_UNPLUG;
    } else {
        return -1;
    }
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    if (plugin_relay_opt) {
        *(int32_t*)data = PLUGIN_RELAY;
    }else if (plugin_stun_opt) {
        *(int32_t*)data = PLUGIN_STUN;
    }else if (plugin_vpn_opt) {
        *(int32_t*)data = PLUGIN_VPN;
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

static int plugin_req_cmd_pack(char* data)
{
    int sz = 0;

    if (!plugin_req_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_PLUGIN_REQ;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    if (plugin_relay_opt) {
        *(int32_t*)data = PLUGIN_RELAY;
    } else if (plugin_stun_opt) {
        *(int32_t*)data = PLUGIN_STUN;
    } else if (plugin_vpn_opt) {
        *(int32_t*)data = PLUGIN_VPN;
    } else {
        return -1;
    }
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    return sz;
}

static int join_node_opt = 0;
static int drop_node_opt = 0;
static int join_node_opt_parse(void)
{
    join_node_opt = 1;
    return 0;
}
static int drop_node_opt_parse(void)
{
    drop_node_opt = 1;
    return 0;
}

static int join_node_cmd = 0;
static int join_node_opt_check(void)
{
    if (join_node_opt) {
        if (!addr_opt) {
            printf("Few arguments\n");
            return -1;
        }
        join_node_cmd = 1;
    }
    return 0;
}
static int drop_node_cmd = 0;
static int drop_node_opt_check(void)
{
    if (drop_node_opt) {
        if (!addr_opt) {
            printf("Few arguments\n");
            return -1;
        }
        drop_node_cmd = 1;
    }
    return 0;
}

static int join_node_cmd_pack(char* data)
{
    int sz = 0;

    if (!join_node_cmd) {
        return -1;
    }

    *(int32_t*)data = VLSCTL_NODE_JOIN;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    *(int32_t*)data = addr_port;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    strcpy(data, addr_ip);
    sz   += strlen(addr_ip) + 1;
    return sz;
}

static int drop_node_cmd_pack(char* data)
{
    int sz = 0;

    if (!drop_node_cmd) {
        return -1;
    }

    *(int32_t*)data = VLSCTL_NODE_DROP;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    *(int32_t*)data = addr_port;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    strcpy(data, addr_ip);
    sz   += strlen(addr_ip) + 1;
    return sz;
}

static int dht_query_opt = 0;
static int dht_query_opt_parse(void)
{
    dht_query_opt = 1;
    return 0;
}

static int dht_ping_opt = 0;
static int dht_ping_opt_parse(void)
{
    dht_ping_opt = 1;
    return 0;
}

static int dht_find_node_opt = 0;
static int dht_find_node_opt_parse(void)
{
    dht_find_node_opt = 1;
    return 0;
}

static int dht_find_closest_nodes_opt = 0;
static int dht_find_closest_nodes_opt_parse(void)
{
    dht_find_closest_nodes_opt = 1;
    return 0;
}

static int dht_post_hash_opt = 0;
static int dht_post_hash_opt_parse(void)
{
    dht_post_hash_opt = 1;
    return 0;
}
static int dht_get_peers_opt = 0;
static int dht_get_peers_opt_parse(void)
{
    dht_get_peers_opt = 1;
    return 0;
}

static int dht_query_cmd = 0;
static int dht_opt_check(void)
{
    if (!dht_query_opt) {
        if (dht_ping_opt ||
            dht_find_node_opt ||
            dht_find_closest_nodes_opt ||
            dht_post_hash_opt ||
            dht_get_peers_opt) {
            printf("Redundant argument\n");
            return -1;
        }
    } else {
        if ((!dht_ping_opt) &&
            (!dht_find_node_opt) &&
            (!dht_find_closest_nodes_opt) &&
            (!dht_post_hash_opt) &&
            (!dht_get_peers_opt)) {
            printf("Few arguments\n");
            return -1;
        }
        if ((dht_ping_opt && dht_find_node_opt) ||
           (dht_ping_opt && dht_find_closest_nodes_opt) ||
           (dht_ping_opt && dht_post_hash_opt) ||
           (dht_ping_opt && dht_get_peers_opt) ||
           (dht_find_node_opt && dht_find_closest_nodes_opt) ||
           (dht_find_node_opt && dht_post_hash_opt) ||
           (dht_find_node_opt && dht_get_peers_opt) ||
           (dht_find_closest_nodes_opt && dht_post_hash_opt) ||
           (dht_find_closest_nodes_opt && dht_get_peers_opt) ||
           (dht_post_hash_opt && dht_get_peers_opt)) {
           printf("Redundant arguments\n");
           return -1;
        }
        if (!addr_opt) {
            printf("Few arguments\n");
            return -1;
        }
        dht_query_cmd = 1;
    }
    return 0;
}

static int dht_query_cmd_pack(char* data)
{
    int sz = 0;
    if (!dht_query_cmd) {
        return -1;
    }
    *(int32_t*)data = VLSCTL_DHT_QUERY;
    data += sizeof(int32_t);
    sz   += sizeof(int32_t);

    if (dht_ping_opt) {
        *(int32_t*)data = VDHT_PING;
    }else if (dht_find_node_opt) {
        *(int32_t*)data = VDHT_FIND_NODE;
    }else if (dht_find_closest_nodes_opt) {
        *(int32_t*)data = VDHT_FIND_CLOSEST_NODES;
    }else if (dht_post_hash_opt) {
        *(int32_t*)data = VDHT_POST_HASH;
    }else if (dht_get_peers_opt) {
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

static int show_help_opt = 0;
static int show_help_opt_parse(void)
{
    show_help_opt = 1;
    return 0;
}

static void show_help_cmd(void)
{
    show_usage();
    return ;
}

static int show_version_opt = 0;
static int show_version_opt_parse(void)
{
    show_version_opt = 1;
    return 0;
}

static void show_version_cmd(void)
{
    printf("Version: 0.0.1\n");
    return ;
}

struct opt_routine {
    char opt;
    int (*parse_cb)(void);
};

struct opt_routine opt_rts[] = {
    {'U', lsctlc_socket_param      },
    {'S', lsctls_socket_param      },
    {'d', host_online_opt_parse    },
    {'D', host_offline_opt_parse   },
    {'x', host_exit_opt_parse      },
    {'s', host_dump_opt_parse      },
    {'c', config_dump_opt_parse    },
    {'a', join_node_opt_parse      },
    {'e', drop_node_opt_parse      },
    {'p', plugin_opt_parse         },
    {'R', plugin_relay_opt_parse   },
    {'T', plugin_stun_opt_parse    },
    {'P', plugin_vpn_opt_parse     },
    {'u', plugin_up_opt_parse      },
    {'w', plugin_down_opt_parse    },
    {'m', addr_opt_parse           },
    {'q', plugin_req_opt_parse     },
    {'t', dht_query_opt_parse      },
    {'J', dht_ping_opt_parse       },
    {'K', dht_find_node_opt_parse  },
    {'L', dht_find_closest_nodes_opt_parse },
    {'M', dht_post_hash_opt_parse  },
    {'N', dht_get_peers_opt_parse  },
    {'v', show_version_opt_parse   },
    {'h', show_help_opt_parse      },
    {0, 0}
};

int (*check_routine[])(void) = {
    lsctlc_socket_check,
    lsctls_socket_check,
    host_opt_check,
    plugin_opt_check,
    join_node_opt_check,
    drop_node_opt_check,
    dht_opt_check,
    NULL
};

int (*cmd_routine[])(char*) = {
    host_online_cmd_pack,
    host_offline_cmd_pack,
    host_exit_cmd_pack,
    host_dump_cmd_pack,
    config_dump_cmd_pack,
    plugin_cmd_pack,
    plugin_req_cmd_pack,
    join_node_cmd_pack,
    drop_node_cmd_pack,
    dht_query_cmd_pack,
    NULL,
};

static char  data[1024];
static char* buf = data;
static int   nitems = 0;

int main(int argc, char** argv)
{
    int ret = 0;
    int i = 0;

    if (argc <= 1) {
        printf("Few arguments\n");
        show_usage();
        exit(-1);
    }
    {
        int opt_idx = 0;
        int c = 0;
        int i = 0;

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
                struct opt_routine* routine = opt_rts;
                for (i = 0; routine[i].parse_cb; i++) {
                    if (routine[i].opt != c) {
                        continue;
                    }
                    ret = routine[i].parse_cb();
                    if (ret < 0) {
                        exit(-1);
                    }
                    break;
                }
                if (!routine[i].parse_cb) {
                    printf("Invalid Arguments\n");
                    exit(-1);
                }
            }
         }
     }

     {
        int (**routine)(void) = check_routine;
        for (i = 0; (routine[i] != 0); i++) {
            ret = routine[i]();
            if (ret < 0) {
                exit(-1);
            }
        }

        if (show_help_opt) {
            show_help_cmd();
            return 0;
        }
        if (show_version_opt) {
            show_version_cmd();
            return 0;
        }
     }

     {
        int (**routine)(char*) = cmd_routine;
        int32_t* nitems_addr = NULL;

        memset(buf, 0, 1024);
        *(uint32_t*)buf = 0x7fec45fa; //lsctl magic;
         buf += sizeof(uint32_t);

        *(int32_t*)buf = 0x45; //VMSG_LSCTL;
        buf += sizeof(int32_t);

        nitems_addr = (int32_t*)buf;
        buf += sizeof(int32_t);

        for (i = 0; routine[i]; i++) {
            ret = routine[i](buf);
            if (ret < 0) {
                continue;
            }
            nitems++;
            buf += ret;
        }
        *(int32_t*)nitems_addr = nitems;
     }

     {
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

