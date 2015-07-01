#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include "vdhtapi.h"
#include "vhashgen.h"

#define FAKE_SERVICE_MAGIC \
    "@elastos-fake@tzl@kortide.orp@fake-service@f66e053099614dbdc93ad" \
    "4fa4556fa3fe9a9e65589900c742331030598d704cf3519ffce7b4d7c4478413"

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
    {"fake-service-up",     no_argument,       0,        'r'},
    {"fake-service-down",   no_argument,       0,        'R'},
    {"fake-service-find",   no_argument,       0,        'f'},
    {"fake-service-probe",  no_argument,       0,        'p'},
    {"ping",                no_argument,       0,        't'},
    {"addr",                required_argument, 0,        'm'},
    {"proto",               required_argument, 0,        'l'},
    {"type",                required_argument, 0,        'y'},
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
    printf("  -r, --fake-service-up   --addr=IP:PORT  --proto=tcp[,udp], --type=host[,upnp, rflx, relay]\n");
    printf("                                            request to post a fake service\n");
    printf("  -R, --fake-service-down --addr=IP:PORT    request to unpost a fake service\n");
    printf("  -f, --fake-service-find                   request to find fake service in vdhtd\n");
    printf("  -p, --fake-service-probe                  request to probe fake service in neighbor nodes\n");;
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

#define VCMD_MASK_ADDR  ((uint32_t)0x0100)
#define VCMD_MASK_HASH  ((uint32_t)0x0200)

enum {
    VCMD_RESERVED       = 0,
    VCMD_START_HOST     = (uint32_t)0x0001,
    VCMD_STOP_HOST      = (uint32_t)0x0002,
    VCMD_MAKE_HOST_EXIT = (uint32_t)0x0003,
    VCMD_DUMP_HOST      = (uint32_t)0x0004,
    VCMD_DUMP_CFG       = (uint32_t)0x0005,
    VCMD_JOIN_NODE      = (uint32_t)0x0006 | VCMD_MASK_ADDR,
    VCMD_BOGUS_PING     = (uint32_t)0x0007 | VCMD_MASK_ADDR,
    VCMD_POST_SERVICE   = (uint32_t)0x0008 | VCMD_MASK_ADDR | VCMD_MASK_HASH,
    VCMD_UNPOST_SERVICE = (uint32_t)0x0009 | VCMD_MASK_ADDR | VCMD_MASK_HASH,
    VCMD_FIND_SERVICE   = (uint32_t)0x000A | VCMD_MASK_HASH,
    VCMD_PROBE_SERVICE  = (uint32_t)0x000B | VCMD_MASK_HASH
};

static int  glsctlc_cmd = 0;

static char glsctlc_socket[256];
static char glsctls_socket[256];
static int  glsctlc_addr_opt  = 0;
static int  glsctlc_proto_opt = 0;
static int  glsctlc_type_opt  = 0;

static struct sockaddr_in glsctlc_addr;
static int      glsctlc_proto = VPROTO_UDP;
static uint32_t glsctlc_type  = VSOCKADDR_LOCAL;
static vsrvcHash glsctlc_hash;

static
int _aux_parse_sockaddr_param(void)
{
    const char* addr = NULL;
    char ip[64];
    int  port = 0;
    int  ret = 0;

    if (strlen(optarg) + 1 >= 64) {
        printf("Invalid IP\n");
        return -1;
    }

    addr = strchr(optarg, ':');
    if (!addr) {
        printf("Invalid IP\n");
        return -1;
    }
    if ((addr - optarg + 1) >= 64) {
        printf("Invalid IP\n");
        return -1;
    }
    memset(ip, 0, 64);
    strncpy(ip, optarg, (int)(addr-optarg));

    errno = 0;
    port = strtol(addr + 1, NULL, 10);
    if (errno) {
        printf("Invalid IP\n");
        return -1;
    }

    glsctlc_addr.sin_family = AF_INET;
    glsctlc_addr.sin_port = htons(port);
    ret = inet_aton(ip, (struct in_addr*)&glsctlc_addr.sin_addr);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

static
int _aux_parse_proto_param(void)
{
    if (strlen(optarg) + 1 >= 8) {
        printf("Invalid protocol\n");
        return -1;
    }
    if (!strcmp(optarg, "udp")) {
        glsctlc_proto = VPROTO_UDP;
    } else if (!strcmp(optarg, "tcp")) {
        glsctlc_proto = VPROTO_TCP;
    } else {
        glsctlc_proto = VPROTO_UNKNOWN;
        return -1;
    }
    return 0;
}

static
int _aux_parse_type_param(void)
{
    if (strlen(optarg) + 1 >= 8) {
        printf("Invalid address type\n");
        return -1;
    }
    if (!strcmp(optarg, "host")) {
        glsctlc_type = VSOCKADDR_LOCAL;
    }else if (!strcmp(optarg, "upnp")) {
        glsctlc_type = VSOCKADDR_UPNP;
    }else if (!strcmp(optarg, "rflx")) {
        glsctlc_type = VSOCKADDR_REFLEXIVE;
    }else if (!strcmp(optarg, "relay")) {
        glsctlc_type = VSOCKADDR_RELAY;
    } else {
        glsctlc_type = VSOCKADDR_UNKNOWN;
        return -1;
    }
    return 0;
}

static
void _aux_print_addr_num_cb(vsrvcHash* hash, int num, int proto, void* cookie)
{
    char* sproto = NULL;

    switch(proto) {
    case VPROTO_UDP:
        sproto = "udp";
        break;
    case VPROTO_TCP:
        sproto = "tcp";
        break;
    default:
        sproto = "err";
        break;
    }
    printf("addr num: %d, proto:%s.\n", num, sproto);
    return ;
}

static
void _aux_print_service_addrs_cb(vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int last, void* cookie)
{
    char* hostname = NULL;
    int port = 0;

    hostname = inet_ntoa((struct in_addr)addr->sin_addr);
    if (!hostname) {
        printf("Wrong address\n");
        return;
    }
    port = (int)ntohs(addr->sin_port);
    printf("address: %s:%d, type:%ud last:%s\n", hostname, port, type, last ? "yes":"no");
    return ;
}

int main(int argc, char** argv)
{
    int opt_idx = 0;
    int ret = 0;
    int c = 0;

    memset(glsctlc_socket, 0, 256);
    memset(glsctls_socket, 0, 256);
    strcpy(glsctlc_socket, "/var/run/vdht/lsctl_client");
    strcpy(glsctls_socket, "/var/run/vdht/lsctl_socket");

    if (argc <= 1) {
        printf("Few arguments\n");
        show_usage();
        exit(-1);
    }

    while(c >= 0) {
        c = getopt_long(argc, argv, "U:S:dDxscarRfptm:l:y:vh", long_options, &opt_idx);
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
            switch(c) {
            case 'd':
                glsctlc_cmd = VCMD_START_HOST;
                break;
            case 'D':
                glsctlc_cmd = VCMD_STOP_HOST;
                break;
            case 'x':
                glsctlc_cmd = VCMD_MAKE_HOST_EXIT;
                break;
            case 's':
                glsctlc_cmd = VCMD_DUMP_HOST;
                break;
            case 'c':
                glsctlc_cmd = VCMD_DUMP_CFG;
                break;
            case 'a':
                glsctlc_cmd = VCMD_JOIN_NODE;
                break;
            case 'r':
                glsctlc_cmd = VCMD_POST_SERVICE;
                break;
            case 'R':
                glsctlc_cmd = VCMD_UNPOST_SERVICE;
                break;
            case 'f':
                glsctlc_cmd = VCMD_FIND_SERVICE;
                break;
            case 'p':
                glsctlc_cmd = VCMD_PROBE_SERVICE;
                break;
            case 't':
                glsctlc_cmd = VCMD_BOGUS_PING;
                break;
            case 'U':
                if (strlen(optarg) + 1 >= 256) {
                    printf("Too long for option 'unix-domain-file' \n");
                    exit(-1);
                }
                memset(glsctlc_socket, 0, 256);
                strcpy(glsctlc_socket, optarg);
                break;
            case 'S':
                if (strlen(optarg) + 1 >= 256) {
                    printf("Too long for option '--lsctl-socket-file' \n");
                    exit(-1);
                }
                memset(glsctls_socket, 0, 256);
                strcpy(glsctls_socket, optarg);
                break;
            case 'm':
                ret = _aux_parse_sockaddr_param();
                if (ret < 0) {
                    printf("Bad address\n");
                    exit(-1);
                }
                glsctlc_addr_opt = 1;
                break;
            case 'l':
                ret = _aux_parse_proto_param();
                if (ret < 0) {
                    printf("Bad protocol\n");
                    exit(-1);
                }
                glsctlc_proto_opt = 1;
                break;
            case 'y':
                ret = _aux_parse_type_param();
                if (ret < 0) {
                    printf("Bad address type\n");
                    exit(-1);
                }
                glsctlc_type_opt = 1;
                break;
            case 'v':
                if (glsctlc_cmd) {
                    printf("Confused command, only one demand allowed\n");
                    exit(-1);
                }
                show_version();
                exit(0);
                break;
            case 'h':
                if (glsctlc_cmd) {
                    printf("Confused command, only one demand allowed\n");
                    exit(-1);
                }
                show_usage();
                exit(0);
                break;
            }
        }
    }
    if (optind < argc) {
        printf("Too many arguments.\n");
        show_usage();
        exit(-1);
    }

    if (glsctlc_cmd <= 0) {
        printf("Confused command, at least one demand needed\n");
        exit(-1);
    }
    if ((glsctlc_cmd & VCMD_MASK_ADDR) && (!glsctlc_addr_opt)) {
        printf("address argument is missing.\n");
        exit(-1);
    }
    if (glsctlc_cmd & VCMD_MASK_HASH) {
        const char* magic = FAKE_SERVICE_MAGIC;
        struct vhashgen hashgen;
        ret = vhashgen_init(&hashgen);
        if (ret < 0) {
            printf("generate fake sevice hash failed\n");
            exit(-1);
        };
        ret = hashgen.ops->hash(&hashgen, (uint8_t*)magic, strlen(magic), &glsctlc_hash);
        vhashgen_deinit(&hashgen);
        if (ret < 0) {
            printf("generate fake service hash failed\n");
            exit(-1);
        }
    }
    vdhtc_init(glsctlc_socket, glsctls_socket);

    switch(glsctlc_cmd) {
    case VCMD_START_HOST:
        ret = vdhtc_start_host();
        break;
    case VCMD_STOP_HOST:
        ret = vdhtc_stop_host();
        break;
    case VCMD_MAKE_HOST_EXIT:
        ret = vdhtc_make_host_exit();
        break;
    case VCMD_DUMP_HOST:
        ret = vdhtc_dump_host_infos();
        break;
    case VCMD_DUMP_CFG:
        ret = vdhtc_dump_cfg_infos();
        break;
    case VCMD_JOIN_NODE:
        ret = vdhtc_join_wellknown_node(&glsctlc_addr);
        break;
    case VCMD_BOGUS_PING:
        ret = vdhtc_request_bogus_ping(&glsctlc_addr);
        break;
    case VCMD_POST_SERVICE:
        ret = vdhtc_post_service_segment(&glsctlc_hash, &glsctlc_addr, glsctlc_type, glsctlc_proto);
        break;
    case VCMD_UNPOST_SERVICE:
        ret = vdhtc_unpost_service_segment(&glsctlc_hash, &glsctlc_addr);
        break;
    case VCMD_FIND_SERVICE:
        ret = vdhtc_find_service(&glsctlc_hash, _aux_print_addr_num_cb, _aux_print_service_addrs_cb, NULL);
        break;
    case VCMD_PROBE_SERVICE:
        ret = vdhtc_probe_service(&glsctlc_hash, _aux_print_addr_num_cb, _aux_print_service_addrs_cb, NULL);
        break;
    default:
        printf("Invalid command.\n");
        break;
    }
    vdhtc_deinit();
    if (ret < 0) {
        printf("command execution failed.\n");
    }else {
        printf("command execution succeed.\n");
    }
    return 0;
}

