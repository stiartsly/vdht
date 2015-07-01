#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include "vdhtapi.h"

static char* gserver_magic = \
     "@example-service@zhilong@kortide-corp@example@765fce110adcb92685cfe4"\
     "a2e990cc02ae361f483a2ad2929498e0e2d06abc46c98a58d36611f9ebe1";

static char  gclient_socket[256];
static char  glsctls_socket[256];
static int   gclient_port = 0;
static int   gserver_proto = VPROTO_UDP;
static int   gserver_help = 0;

static
struct option long_options[] = {
    {"unix-domain-file",  required_argument, 0,  'U' },
    {"lsctl-socket-file", required_argument, 0,  'S' },
    {"port",              required_argument, 0,  'p' },
    {"help",              no_argument,       0,  'h' },
    {0, 0, 0, 0}
};

void show_usage(void)
{
    printf("Usage: vdhtd [OPTION...]\n");
    printf("  --unix-domain-file=UNIX_DOMAIN_FILE     unix domain path to communicate with vdhtd\n");
    printf("  --lsctl-socket-file=LSCTL_FILE          unix domain path for vdhtd to use\n");
    printf("  -p, --port=PORT                         port to use for communication\n");
    printf("\n");
    printf("Help options\n");
    printf("  -h  --help                              show this help message.\n");
    printf("\n");
    return ;
}

void vexample_dump_addr(struct sockaddr_in* addr)
{
    char* hostname = NULL;
    int port = 0;

    hostname = inet_ntoa((struct in_addr)addr->sin_addr);
    if (!hostname) {
        printf("inet_ntoa failed\n");
        return ;
    }
    port = (int)ntohs(addr->sin_port);
    printf("dump addr: (%s:%d)\n", hostname, port);
    return ;
}

static
int vexample_get_hash(vsrvcHash* hash)
{
    struct vhashgen hashgen;
    int ret = 0;
    ret = vhashgen_init(&hashgen);
    if (ret < 0) {
        return -1;
    }
    ret = hashgen.ops->hash(&hashgen, (uint8_t*)gserver_magic, strlen(gserver_magic), hash);
    vhashgen_deinit(&hashgen);
    if (ret < 0) {
        return -1;
    }
    return 0;
}

static
int vexample_get_addr(struct sockaddr_in* addr)
{
    struct ifconf ifc;
    struct ifreq  ifr[16];
    int index = 0;
    struct ifreq* req = NULL;
    int fd = 0;
    int no_ifs = 0;
    int ret = 0;
    int yes = 0;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket:");
        return -1;
    }
    index = 0;
    memset(ifr, 0, sizeof(ifr));
    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_buf = (caddr_t)ifr;

    ret = ioctl(fd, SIOCGIFCONF, &ifc);
    if (ret < 0) {
        perror("ioctl:");
        close(fd);
        return -1;
    }
    close(fd);

    req = &ifr[index];
    yes = !strncmp(req->ifr_name, "eth", 3) || !strncmp(req->ifr_name, "wlan", 4);
    while (!yes) {
        index++;
        if (index >= ifc.ifc_len/sizeof(struct ifreq)) {
            no_ifs = 1;
            break;
        }
        req = &ifr[index];
        yes = !strncmp(req->ifr_name, "eth", 3) || !strncmp(req->ifr_name, "wlan", 4);
    }
    if (no_ifs) {
        printf("no ip address\n");
        return -1;
    }

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(gclient_port);
    addr->sin_addr = ((struct sockaddr_in*)&(req->ifr_addr))->sin_addr;

    return 0;
}

static
int vexample_loop_run_with_udp(struct sockaddr_in* addr, struct sockaddr_in* to)
{
    struct sockaddr_in from_addr;
    char data[32];
    int quit = 0;
    int ask  = 1;
    int err = 0;
    int ret = 0;
    int fd  = 0;

    srand(time(NULL));

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket:");
        return -1;
    }
    ret = bind(fd, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        perror("bind:");
        close(fd);
        return -1;
    }

    if (ask) {
        char a = 'o';
        ret = sendto(fd, &a, 1, 0, (struct sockaddr*)to, sizeof(struct sockaddr_in));
        if (ret < 0) {
            perror("sendto:");
            close(fd);
            return -1;
        }
        ask = 0;
    }

    while(!quit) {
        struct timespec tmo = {0, 5000*1000};
        sigset_t sigmask;
        sigset_t origmask;
        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        int ret = 0;

        sigemptyset(&sigmask);
        sigemptyset(&origmask);
        pthread_sigmask(SIG_SETMASK, &sigmask, &origmask);

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        FD_SET(fd, &rfds);
        FD_SET(fd, &wfds);
        FD_SET(fd, &efds);

        ret = pselect(fd + 1, &rfds, &wfds, &efds, &tmo, &sigmask);
        if (ret < 0) {
            printf("pselect:");
            err = 1;
            break;
        } else if (ret == 0) {
            continue;
        } else {
            if (FD_ISSET(fd, &rfds)) {
                int len = 0;
                memset(data, 0, 32);
                ret = recvfrom(fd, data, 32, 0, (struct sockaddr*)&from_addr, (socklen_t*)&len);
                if (ret < 0) {
                    perror("recvfrom:");
                    err = 1;
                    break;
                }
                if (ret > 0) {
                    char ch = (char)data[0];
                    if (ch == 'x') {
                        quit = 1;
                        break;
                    } else {
                        printf("%c", (char)data[0]);
                        ask = 1;
                    }
                }
            }
            if (FD_ISSET(fd, &wfds)) {
                if (ask) {
                    char a = 'o';
                    ret = sendto(fd, &a, 1, 0, (struct sockaddr*)to, sizeof(struct sockaddr_in));
                    if (ret < 0) {
                        perror("sendto:");
                        err = 1;
                        break;
                    }
                    ask = 0;
                }
            }
            if (FD_ISSET(fd, &efds)) {
                printf("pselect error\n");
                err = 1;
                break;
            }
        }
    }
    close(fd);
    return (err) ? -1 : 0;
}

static
int vexample_loop_run_with_tcp(struct sockaddr_in* addr, struct sockaddr_in* to)
{
    char data[32];
    int quit = 0;
    int ask  = 1;
    int err = 0;
    int ret = 0;
    int fd  = 0;

    srand(time(NULL));

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    ret = bind(fd, (struct sockaddr*)addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    ret = connect(fd, (struct sockaddr*)to, sizeof(*to));
    if (ret < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    if (ask) {
        char a = 'o';
        ret = send(fd, &a, 1, 0);
        if (ret < 0) {
            perror("send:");
            close(fd);
            return -1;
        }
        ask = 0;
    }

    while(!quit) {
        struct timespec tmo = {0, 5000*1000};
        sigset_t sigmask;
        sigset_t origmask;
        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        int ret = 0;

        sigemptyset(&sigmask);
        sigemptyset(&origmask);
        pthread_sigmask(SIG_SETMASK, &sigmask, &origmask);

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        FD_SET(fd, &rfds);
        FD_SET(fd, &wfds);
        FD_SET(fd, &efds);

        ret = pselect(fd + 1, &rfds, &wfds, &efds, &tmo, &sigmask);
        if (ret < 0) {
            printf("pselect:");
            err = 1;
            break;
        } else if (ret == 0) {
            continue;
        } else {
            if (FD_ISSET(fd, &rfds)) {
                memset(data, 0, 32);
                ret = recv(fd, data, 32, 0);
                if (ret < 0) {
                    perror("recv:");
                    err = 1;
                    break;
                }
                if (ret > 0) {
                    char ch = (char)data[0];
                    if (ch == 'x') {
                        quit = 1;
                        break;
                    } else {
                        printf("%c", (char)data[0]);
                        ask = 1;
                    }
                }
            }
            if (FD_ISSET(fd, &wfds)) {
                if (ask) {
                    char a = 'o';
                    ret = send(fd, &a, 1, 0);
                    if (ret < 0) {
                        perror("send:");
                        err = 1;
                        break;
                    }
                    ask = 0;
                }
            }
            if (FD_ISSET(fd, &efds)) {
                printf("pselect error\n");
                err = 1;
                break;
            }
        }
    }
    close(fd);
    return (err) ? -1 : 0;
}

void vexample_number_addr_cb(vsrvcHash* hash, int num, int proto, void* cookie)
{
    char* sproto = NULL;

    switch(proto) {
    case VPROTO_UDP:
        sproto = "udp";
        gserver_proto = VPROTO_UDP;
        break;
    case VPROTO_TCP:
        sproto = "tcp";
        gserver_proto = VPROTO_TCP;
        break;
    default:
        sproto = "err";
        break;
    }
    printf("addr num:%d (proto:%s).\n", num, sproto);
    return ;
}

void vexample_iterate_addr_cb(vsrvcHash* hash, struct sockaddr_in* addr, uint32_t type, int last, void* cookie)
{
    struct sockaddr_in* server = (struct sockaddr_in*)cookie;
    vexample_dump_addr(addr);
    printf("type: %u\n", type);
    printf("last: %d.\n", last);

    memcpy(server, addr, sizeof(*addr));
    return ;
}

int main(int argc, char** argv)
{
    int opt_idx = 0;
    int ret = 0;
    int c = 0;

    memset(gclient_socket, 0, 256);
    memset(glsctls_socket, 0, 256);
    strcpy(gclient_socket, "/var/run/vdht/lsctl_client_example");
    strcpy(glsctls_socket, "/var/run/vdht/lsctl_socket");

    if (argc <= 1) {
        printf("Few arguments\n");
        show_usage();
        exit(-1);
    }

    while(c >= 0) {
        c = getopt_long(argc, argv, "U:S:p:h", long_options, &opt_idx);
        if (c < 0) {
            break;
        }
        switch(c) {
        case 0: {
            if (long_options[opt_idx].flag != 0)
                break;
            printf ("option %s", long_options[opt_idx].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
        }
        case 'U':
            if (strlen(optarg) + 1 >= 256) {
                printf("Too long for option 'unix-domain-file' \n");
                exit(-1);
            }
            memset(gclient_socket, 0, 256);
            strcpy(gclient_socket, optarg);
            break;
        case 'S':
            if (strlen(optarg) + 1 >= 256) {
                printf("Too long for option 'lsctl-socket-file\n");
                exit(-1);
            }
            memset(glsctls_socket, 0, 256);
            strcpy(glsctls_socket, optarg);
            break;
        case 'p': {
            char port_buf[32] = {0};
            if (strlen(optarg) + 1 >= 32) {
                printf("Too long for option 'port'\n");
                exit(-1);
            }
            memset(port_buf, 0, 32);
            strcpy(port_buf, optarg);
            errno = 0;
            gclient_port = strtol(port_buf, NULL, 10);
            if (errno) {
                printf("Wrong option for 'port'\n");
                exit(-1);
            }
            break;
        }
        case 'h':
            gserver_help = 1;
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

    if (gserver_help) {
        show_usage();
        exit(0);
    }

    {
        struct sockaddr_in laddr;
        struct sockaddr_in to;
        vsrvcHash srvcHash;

        ret = vexample_get_hash(&srvcHash);
        if (ret < 0) {
            printf("failed to get service hash\n");
            exit(-1);
        }
        ret = vexample_get_addr(&laddr);
        if (ret < 0) {
            printf("failed to get local addr.\n");
            exit(-1);
        }
        vexample_dump_addr(&laddr);

        vdhtc_init(gclient_socket, glsctls_socket);
        ret = vdhtc_find_service(&srvcHash, vexample_number_addr_cb, vexample_iterate_addr_cb, &to);
        if (ret < 0) {
            printf("post service failed.\n");
            vdhtc_deinit();
            exit(-1);
        }

        switch(gserver_proto) {
        case VPROTO_UDP:
            ret = vexample_loop_run_with_udp(&laddr, &to);
            break;
        case VPROTO_TCP:
            ret = vexample_loop_run_with_tcp(&laddr, &to);
            break;
        default:
            vdhtc_deinit();
            printf("Bad protocol\n");
            exit(-1);
        }
        if (ret < 0) {
            printf("failed to run server.\n");
            vdhtc_deinit();
            exit(-1);
        }

        vdhtc_deinit();
    }
    return 0;
}

