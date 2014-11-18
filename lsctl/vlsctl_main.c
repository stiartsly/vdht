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

static char* cur_unix_path = "/var/run/vdht/lsctl_client";
static int def_unix_path = 1;
static char unix_path[1024];
static int host_dump  = 0;
static int cfg_dump   = 0;
static int host_up    = 0;
static int host_down  = 0;
static int host_quit  = 0;

static char node_ip[64];
static int  node_port = 0;
static int add_node   = 0;
static int del_node   = 0;

static char relay_ip[64];
static int relay_port = 0;
static int relay_up   = 0;
static int relay_down = 0;

static char stun_ip[64];
static int stun_port  = 0;
static int stun_up    = 0;
static int stun_down  = 0;

static char vpn_ip[64];
static int vpn_port   = 0;
static int vpn_up     = 0;
static int vpn_down   = 0;

static char dest_ip[64];
static int dest_port  = 0;
static int ping       = 0;
static int find_node  = 0;
static int find_nodes = 0;

static int show_help  = 0;
static int show_ver   = 0;

static
struct option long_options[] = {
    {"unix-path",          required_argument,  0,         'U'},
    {"cfg-dump",           no_argument,        &cfg_dump,   1},
    {"host-up",            no_argument,        &host_up,    1},
    {"host-down",          no_argument,        &host_down,  1},
    {"host-quit",          no_argument,        &host_quit,  1},
    {"host-dump",          no_argument,        &host_dump,  1},
    {"add-node",           required_argument,  0,         'a'},
    {"del-node",           required_argument,  0,         'e'},
    {"relay_up",           no_argument,        &relay_up,   1},
    {"relay_down",         no_argument,        &relay_down, 1},
    {"stun-up",            no_argument,        &stun_up,    1},
    {"stun-down",          no_argument,        &stun_down,  1},
    {"vpn_up",             no_argument,        &vpn_up,     1},
    {"vpn-down",           no_argument,        &vpn_down,   1},
    {"ping",               required_argument,  0,         'l'},
    {"find_node",          required_argument,  0,         'm'},
    {"find_closest_nodes", required_argument,  0,         'n'},
    {"help",               no_argument,        &show_help,  1},
    {"version",            no_argument,        &show_ver,   1},
    {0, 0, 0, 0}
};

void show_usage(void)
{
    printf("Usage: vlsctl [OPTION...]\n");
    printf("  -U, --unix-path=FILE              unix path for communicating with vdhtd\n");
    printf("  -C  --cfg-dump                    request to dump config\n");
    printf("  -d, --host-up                     request to start host\n");
    printf("  -D, --host-down                   request to stop host\n");
    printf("  -X, --host-quit                   request to shutdown host\n");
    printf("  -S, --host-dump                   request to dump host\n");
    printf("  -a  --add-node=IP:PORT            reqeust to add wellknown node\n");
    printf("  -e  --del-node=IP:PORT            reqeust to delete wellknown node\n");
    printf("  -r, --relay_up=IP:PORT            request to plug relay service\n");
    printf("  -R, --relay_down=IP:PORT          request to unplug relay service\n");
    printf("  -t, --stun_up=IP:PORT             request to plug stun service\n");
    printf("  -T, --stun_down=IP:PORT           request to unplug stun service\n");
    printf("  -p, --vpn_up=IP:PORT              request to plug vpn service\n");
    printf("  -P, --vpn_down=IP:PORT            request to unplug vpn service\n");
    printf("  -l, --ping=IP:PORT                request to send ping query.\n");
    printf("  -m, --find_node=IP:PORT           request to send find_node query.\n");
    printf("  -n, --find_closest_nodes=IP:PORT  request to send find_closest_nodes query.\n");
    printf("\n");
    printf("Help options\n");
    printf("  -h  --help                        Show this help message.\n");
    printf("  -v, --version                     Print version.\n");
    printf("\n");
}

void show_version(void)
{
    printf("Version 0.0.1\n");
    return ;
}

static struct sockaddr_un dest_addr;

int parse_ip(const char* full_ip, char* ip, int len, int* port)
{
    char* port_addr = (char*)full_ip;
    char* new_addr = NULL;

    port_addr = strchr(full_ip, ':');
    if (!port_addr) {
        return -1;
    }
    if ((port_addr - full_ip) >= len) {
        return -1;
    }

    strncpy(ip, optarg, (int)(port_addr - full_ip));
    port_addr += 1;

    errno = 0;
    *port = strtol(port_addr, &new_addr, 10);
    if (errno) {
        return -1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    struct sockaddr_un unix_addr;
    char  data[1024];
    char* buf  = data;
    void* addr = NULL;
    int opt_idx = 0;
    int nitems = 0;
    int ret = 0;
    int fd = 0;
    int c = 0;

    if (argc <= 1) {
        printf("Few argument\n");
        show_usage();
        exit(-1);
    }

    while(c >= 0) {

        c = getopt_long(argc, argv, "U:SCdDXa:e:r:R:t:T:p:P:l:n:m:hv", long_options, &opt_idx);
        if (c < 0) {
            break;
        }
        switch(c) {
        case 0:
            if (long_options[opt_idx].flag != 0)
                break;
            printf ("option %s", long_options[opt_idx].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
        case 'U':
            def_unix_path = 0;
            if (strlen(optarg) + 1 >= sizeof(unix_path)) {
                printf("Too long unix path.\n");
                exit(-1);
            }
            strcpy(unix_path, optarg);
            break;
        case 'S':
            host_dump = 1;
            break;
        case 'C':
            cfg_dump = 1;
            break;
        case 'd':
            if (host_down) {
                printf("Conflict options\n");
                exit(-1);
            }
            host_up = 1;
            break;
        case 'D':
            if (host_up) {
                printf("Conflict options\n");
                exit(-1);
            }
            host_down = 1;
            break;
        case 'X':
            host_quit = 1;
            break;
        case 'a': {
            if (del_node) {
                printf("Conflict options for '-e' and '-a'\n");
                exit(-1);
            }
            add_node = 1;
            ret = parse_ip(optarg, node_ip, 64, &node_port);
            if (ret < 0){
                printf("Invalid IP or Port\n");
                exit(-1);
            }
            break;
        }
        case 'e':{
            if (add_node) {
                printf("Conflict options for '-e' and '-a'\n");
                exit(-1);
            }
            del_node = 1;
            ret = parse_ip(optarg, node_ip, 64, &node_port);
            if (ret < 0){
                printf("Invalid IP or Port\n");
                exit(-1);
            }
            break;
        }
        case 'r':
            if (relay_down) {
                printf("Conflict options for '-r' and '-R'\n");
                exit(-1);
            }
            relay_up = 1;
            ret = parse_ip(optarg, relay_ip, 64, &relay_port);
            if (ret < 0) {
                printf("Invalid IP or Port.\n");
                exit(-1);
            }
            break;
        case 'R':
            if (relay_up) {
                printf("Conflict options for '-r' and '-R'\n");
                exit(-1);
            }
            relay_down = 1;
            ret = parse_ip(optarg, relay_ip, 64, &relay_port);
            if (ret < 0) {
                printf("Invalid IP or Port.\n");
                exit(-1);
            }
            break;
        case 't':
            if (stun_down) {
                printf("Conflict options for '-t' and '-T'\n");
                exit(-1);
            }
            stun_up = 1;
            ret = parse_ip(optarg, stun_ip, 64, &stun_port);
            if (ret < 0) {
                printf("Invalid IP or Port.\n");
                exit(-1);
            }
            break;
        case 'T':
            if (stun_up) {
                printf("Conflict options for '-t' and '-T'\n");
                exit(-1);
            }
            stun_down = 1;
            ret = parse_ip(optarg, stun_ip, 64, &stun_port);
            if (ret < 0) {
                printf("Invalid IP or Port.\n");
                exit(-1);
            }
            break;
        case 'p':
            if (vpn_down) {
                printf("Conflict options for '-p' and '-P'\n");
                exit(-1);
            }
            vpn_up = 1;
            ret = parse_ip(optarg, vpn_ip, 64, &vpn_port);
            if (ret < 0) {
                printf("Invalid IP or Port.\n");
                exit(-1);
            }
            break;
        case 'P':
            if (vpn_up) {
                printf("Conflict options for '-p' and '-P'\n");
                exit(-1);
            }
            vpn_down = 1;
            ret = parse_ip(optarg, vpn_ip, 64, &vpn_port);
            if (ret < 0) {
                printf("Invalid IP or Port.\n");
                exit(-1);
            }
            break;
        case 'l':
            if (find_node || find_nodes) {
                printf("Too many queries to send\n");
                exit(-1);
            }
            ping = 1;
            ret = parse_ip(optarg, dest_ip, 64, &dest_port);
            if (ret < 0){
                printf("Invalid IP or Port\n");
                exit(-1);
            }
            break;
        case 'm':
            if (ping || find_nodes) {
                printf("Too many queries to send\n");
                exit(-1);
            }
            find_node = 1;
            ret = parse_ip(optarg, dest_ip, 64, &dest_port);
            if (ret < 0){
                printf("Invalid IP or Port\n");
                exit(-1);
           }
            break;
        case 'n':
            if (ping || find_node) {
                printf("Too many queries to send\n");
                exit(-1);
            }
            find_nodes = 1;
            ret = parse_ip(optarg, dest_ip, 64, &dest_port);
            if (ret < 0){
                printf("Invalid IP or Port\n");
                exit(-1);
            }
            break;
        case 'h':
            show_help = 1;
            break;
        case 'v':
            show_ver = 1;
            break;
        default:
            printf("Invalid option -%c: Unknown option.\n", (char)c);
            show_usage();
            exit(-1);
        }
    }
    if (optind < argc) {
        printf("Too many arguments.\n");
        show_usage();
        exit(-1);
    }

    if (show_help) {
        show_usage();
        exit(0);
    }

    if (show_ver) {
        show_version();
        exit(0);
    }

    *(uint32_t*)buf = 0x7fec45fa; //lsctl magic;
    buf += sizeof(uint32_t);

    *(int32_t*)buf = 0x45; //VMSG_LSCTL;
    buf += sizeof(int32_t);

    addr = (int32_t*)buf;
    buf += sizeof(int32_t);
    if (host_dump) {
        *(int32_t*)buf = VLSCTL_HOST_DUMP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (cfg_dump) {
        *(int32_t*)buf = VLSCTL_CFG_DUMP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (host_up) {
        *(int32_t*)buf = VLSCTL_HOST_UP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (host_down) {
        *(int32_t*)buf = VLSCTL_HOST_DOWN;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (host_quit) {
        *(int32_t*)buf = VLSCTL_HOST_EXIT;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (add_node) {
        *(int32_t*)buf = VLSCTL_NODE_JOIN;
        buf += sizeof(int32_t);
        *(int32_t*)buf = node_port;
        buf += sizeof(int32_t);
        strcpy(buf, node_ip);
        buf += strlen(node_ip) + 1;
        nitems++;
    }
    if (del_node) {
        *(int32_t*)buf = VLSCTL_NODE_DROP;
        buf += sizeof(int32_t);
        *(int32_t*)buf = node_port;
        buf += sizeof(int32_t);
        strcpy(buf, node_ip);
        buf += strlen(node_ip) + 1;
        nitems++;
    }
    if (relay_up) {
        *(int32_t*)buf = VLSCTL_PLUG,
        buf += sizeof(int32_t);
        *(int32_t*)buf = PLUGIN_RELAY;
        buf += sizeof(int32_t);
        *(int32_t*)buf = relay_port;
        buf += sizeof(int32_t);
        strcpy(buf, relay_ip);
        buf += strlen(relay_ip) + 1;
        nitems++;
    }
    if (relay_down) {
        *(int32_t*)buf = VLSCTL_UNPLUG,
        buf += sizeof(int32_t);
        *(int32_t*)buf = PLUGIN_RELAY,
        buf += sizeof(int32_t);
        *(int32_t*)buf = relay_port;
        buf += sizeof(int32_t);
        strcpy(buf, relay_ip);
        buf += strlen(relay_ip) + 1;
        nitems++;
    }
    if (stun_up) {
        *(int32_t*)buf = VLSCTL_PLUG;
        buf += sizeof(int32_t);
        *(int32_t*)buf = PLUGIN_STUN;
        buf += sizeof(int32_t);
        *(int32_t*)buf = stun_port;
        buf += sizeof(int32_t);
        strcpy(buf, stun_ip);
        buf += strlen(stun_ip) + 1;
        nitems++;
    }
    if (stun_down) {
        *(int32_t*)buf = VLSCTL_UNPLUG;
        buf += sizeof(int32_t);
        *(int32_t*)buf = PLUGIN_STUN;
        buf += sizeof(int32_t);
        *(int32_t*)buf = stun_port;
        buf += sizeof(int32_t);
        strcpy(buf, stun_ip);
        buf += strlen(stun_ip) + 1;
        nitems++;
    }
    if (vpn_up) {
        *(int32_t*)buf = VLSCTL_PLUG;
        buf += sizeof(int32_t);
        *(int32_t*)buf = PLUGIN_VPN;
        buf += sizeof(int32_t);
        *(int32_t*)buf = vpn_port;
        buf += sizeof(int32_t);
        strcpy(buf, vpn_ip);
        buf += strlen(vpn_ip) + 1;
        nitems++;
    }
    if (vpn_down) {
        *(int32_t*)buf = VLSCTL_UNPLUG;
        buf += sizeof(int32_t);
        *(int32_t*)buf = PLUGIN_VPN;
        buf += sizeof(int32_t);
        *(int32_t*)buf = vpn_port;
        buf += sizeof(int32_t);
        strcpy(buf, vpn_ip);
        buf += strlen(vpn_ip) + 1;
        nitems++;
    }
    if (ping) {
        *(int32_t*)buf = VLSCTL_DHT_QUERY;
        buf += sizeof(int32_t);
        *(int32_t*)buf = VDHT_PING;
        buf += sizeof(int32_t);
        *(int32_t*)buf = dest_port;
        buf += sizeof(int32_t);
        strcpy(buf, dest_ip);
        buf += strlen(dest_ip) + 1;
        nitems++;
    }
    if (find_node) {
        *(int32_t*)buf = VLSCTL_DHT_QUERY;
        buf += sizeof(int32_t);
        *(int32_t*)buf = VDHT_FIND_NODE;
        buf += sizeof(int32_t);
        *(int32_t*)buf = dest_port;
        buf += sizeof(int32_t);
        strcpy(buf, dest_ip);
        buf += strlen(dest_ip) + 1;
        nitems++;
    }
    if (find_nodes) {

        *(int32_t*)buf = VLSCTL_DHT_QUERY;
        buf += sizeof(int32_t);
        *(int32_t*)buf = VDHT_FIND_CLOSEST_NODES;
        buf += sizeof(int32_t);
        *(int32_t*)buf = dest_port;
        buf += sizeof(int32_t);
        strcpy(buf, dest_ip);
        buf += strlen(dest_ip) + 1;
        nitems++;
    }

    *(int32_t*)addr = nitems;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket:");
        exit(-1);
    }
    unlink(cur_unix_path);

    unix_addr.sun_family = AF_UNIX;
    strcpy(unix_addr.sun_path, cur_unix_path);
    ret = bind(fd, (struct sockaddr*)&unix_addr, sizeof(unix_addr));
    if (ret < 0) {
        perror("bind:");
        close(fd);
        exit(-1);
    }

    dest_addr.sun_family = AF_UNIX;
    if (def_unix_path) {
        strcpy(dest_addr.sun_path, "/var/run/vdht/lsctl_socket");
    } else {
        strcpy(dest_addr.sun_path, unix_path);
    }
    ret = sendto(fd, data, (int)(buf-data), 0, (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_un));
    if (ret < 0) {
        perror("sendto:");
        close(fd);
        exit(-1);
    }
    close(fd);
    return 0;
}

