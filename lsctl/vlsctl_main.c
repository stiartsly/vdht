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
    VLSCTL_DHT_UP,
    VLSCTL_DHT_DOWN,
    VLSCTL_DHT_EXIT,

    VLSCTL_ADD_NODE,
    VLSCTL_DEL_NODE,

    VLSCTL_RELAY_UP,
    VLSCTL_RELAY_DOWN,
    VLSCTL_STUN_UP,
    VLSCTL_STUN_DOWN,
    VLSCTL_VPN_UP,
    VLSCTL_VPN_DOWN,

    VLSCTL_LOGOUT,
    VLSCTL_CFGOUT,
    VLSCTL_BUTT
};

static char* cur_unix_path = "/var/run/vdht/lsctl_client";
static int def_unix_path = 1;
static char unix_path[1024];
static int logstdout = 0;
static int cfgstdout = 0;
static int dht_up    = 0;
static int dht_down  = 0;
static int dht_quit  = 0;
static int add_node  = 0;
static int del_node  = 0;
static char node_ip[64];
static int node_port = 0;
static int relay_up  = 0;
static int relay_down = 0;
static int stun_up   = 0;
static int stun_down = 0;
static int vpn_up    = 0;
static int vpn_down  = 0;
static int show_help = 0;
static int show_ver  = 0;

static
struct option long_options[] = {
    {"unix-path",   required_argument,  0,         'U'},
    {"log-stdout",  no_argument,        &logstdout,  1},
    {"cfg-stdout",  no_argument,        &cfgstdout,  1},
    {"dht-up",      no_argument,        &dht_up,     1},
    {"dht-down",    no_argument,        &dht_down,   1},
    {"dht-quit",    no_argument,        &dht_quit,   1},
    {"add-node",    required_argument,  0,         'a'},
    {"del-node",    required_argument,  0,         'e'},
    {"relay_up",    no_argument,        &relay_up,   1},
    {"relay_down",  no_argument,        &relay_down, 1},
    {"stun-up",     no_argument,        &stun_up,    1},
    {"stun-down",   no_argument,        &stun_down,  1},
    {"vpn_up",      no_argument,        &vpn_up,     1},
    {"vpn-down",    no_argument,        &vpn_down,   1},
    {"help",        no_argument,        &show_help,  1},
    {"version",     no_argument,        &show_ver,   1},
    {0, 0, 0, 0}
};

void show_usage(void)
{
    printf("Usage: vlsctl [OPTION...]\n");
    printf("  -U, --unix-path=FILE          unix path for communicating with vdhtd\n");
    printf("  -S, --log-stdout              request to log stdout.\n");
    printf("  -C  --cfg_stdout              request to print config\n");
    printf("  -d, --dht-up                  request to dht up.\n");
    printf("  -D, --dht-down                request to dht down.\n");
    printf("  -X, --dht-quit                request to dht shutdown.\n");
    printf("  -a  --add-node=IP:PORT        reqeust to add wellknown node.\n");
    printf("  -e  --del-node=IP:PORT        reqeust to delete wellknown node.\n");
    printf("  -r, --relay_up        \n");
    printf("  -R, --relay_down      \n");
    printf("  -t, --stun_up         \n");
    printf("  -T, --stun_down       \n");
    printf("  -p, --vpn_up          \n");
    printf("  -P, --vpn_down        \n");
    printf("\n");
    printf("Help options\n");
    printf("  -h  --help                    Show this help message.\n");
    printf("  -v, --version                 Print version.\n");
    printf("\n");
}

void show_version(void)
{
    printf("Version 0.0.1\n");
    return ;
}

static struct sockaddr_un dest_addr;

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

        c = getopt_long(argc, argv, "U:SCdDXa:e:rRtTpPhv", long_options, &opt_idx);
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
            logstdout = 1;
            break;
        case 'C':
            cfgstdout = 1;
            break;
        case 'd':
            if (dht_down) {
                printf("Conflict options\n");
                exit(-1);
            }
            dht_up = 1;
            break;
        case 'D':
            if (dht_up) {
                printf("Conflict options\n");
                exit(-1);
            }
            dht_down = 1;
            break;
        case 'X':
            dht_quit = 1;
            break;
        case 'a': {
            char* port_addr = optarg;
            char* new_addr = NULL;

            if (del_node) {
                printf("Conflict options for '-e' and '-a'\n");
                exit(-1);
            }
            add_node = 1;
            port_addr = strchr(optarg, ':');
            if (!port_addr) {
                printf("Invalid IP\n");
                exit(-1);
            }
            strncpy(node_ip, optarg, (int)(port_addr-optarg));
            port_addr += 1;
            errno = 0;
            node_port = strtol(port_addr, &new_addr, 10);
            if (errno) {
                printf("Invalid Port\n");
                exit(-1);
            }
            break;
        }
        case 'e':{
            char* port_addr = optarg;
            char* new_addr = NULL;

            if (add_node) {
                printf("Conflict options for '-e' and '-a'\n");
                exit(-1);
            }
            del_node = 1;
            port_addr = strchr(optarg, ':');
            if (!port_addr) {
                printf("Invalid IP\n");
                exit(-1);
            }
            strncpy(node_ip, optarg, (int)(port_addr-optarg));
            port_addr += 1;
            errno = 0;
            node_port = strtol(port_addr, &new_addr, 10);
            if (errno) {
                printf("Invalid Port\n");
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
            break;
        case 'R':
            if (relay_up) {
                printf("Conflict options for '-r' and '-R'\n");
                exit(-1);
            }
            relay_down = 1;
            break;
        case 't':
            if (stun_down) {
                printf("Conflict options for '-t' and '-T'\n");
                exit(-1);
            }
            stun_up = 1;
            break;
        case 'T':
            if (stun_up) {
                printf("Conflict options for '-t' and '-T'\n");
                exit(-1);
            }
            stun_down = 1;
            break;
        case 'p':
            if (vpn_down) {
                printf("Conflict options for '-p' and '-P'\n");
                exit(-1);
            }
            vpn_up = 1;
            break;
        case 'P':
            if (vpn_up) {
                printf("Conflict options for '-p' and '-P'\n");
                exit(-1);
            }
            vpn_down = 1;
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

    *(uint32_t*)buf = 0x7fec45fa; //lsctl magic;
    buf += sizeof(uint32_t);

    *(int32_t*)buf = 0x45; //VMSG_LSCTL;
    buf += sizeof(int32_t);

    addr = (int32_t*)buf;
    buf += sizeof(int32_t);
    if (logstdout) {
        *(int32_t*)buf = VLSCTL_LOGOUT;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (cfgstdout) {
        *(int32_t*) buf = VLSCTL_CFGOUT;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (dht_up) {
        *(int32_t*)buf = VLSCTL_DHT_UP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (dht_down) {
        *(int32_t*)buf = VLSCTL_DHT_DOWN;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (dht_quit) {
        *(int32_t*)buf = VLSCTL_DHT_EXIT;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (add_node) {
        *(int32_t*)buf = VLSCTL_ADD_NODE;
        buf += sizeof(int32_t);
        *(int32_t*)buf = node_port;
        buf += sizeof(int32_t);
        strcpy(buf, node_ip);
        buf += strlen(node_ip) + 1;
        nitems++;
    }
    if (del_node) {
        *(int32_t*)buf = VLSCTL_DEL_NODE;
        buf += sizeof(int32_t);
        *(int32_t*)buf = node_port;
        buf += sizeof(int32_t);
        strcpy(buf, node_ip);
        buf += strlen(node_ip) + 1;
        nitems++;
    }
    if (relay_up) {
        *(int32_t*)buf = VLSCTL_RELAY_UP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (relay_down) {
        *(int32_t*)buf = VLSCTL_RELAY_DOWN;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (stun_up) {
        *(int32_t*)buf = VLSCTL_STUN_UP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (stun_down) {
        *(int32_t*)buf = VLSCTL_STUN_DOWN;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (vpn_up) {
        *(int32_t*)buf = VLSCTL_VPN_UP;
        buf += sizeof(int32_t);
        nitems++;
    }
    if (vpn_down) {
        *(int32_t*)buf = VLSCTL_VPN_DOWN;
        buf += sizeof(int32_t);
        nitems++;
    }

    *(int32_t*)addr = nitems;
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

