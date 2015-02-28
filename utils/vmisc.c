#include "vglobal.h"
#include "vmisc.h"

enum {
    VADDR_CLASS_A,
    VADDR_CLASS_B,
    VADDR_CLASS_C,
    VADDR_CLASS_D,
    VADDR_CLASS_E
};

/**
 * @ host: [in, out]
 * @ len:  [in] buffer size;
 * @ port: [out]
 **/
static struct ifconf gifc;
static struct ifreq  gifr[16];
static int gindex = 0;
int vhostaddr_get_first(char* host, int sz)
{
    struct ifreq* req = NULL;
    char* ip = NULL;
    int sockfd = 0;
    int no_ifs = 0;
    int ret = 0;
    int yes = 0;

    vassert(host);
    vassert(sz > 0);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    vlog((sockfd < 0), elog_socket);
    retE((sockfd < 0));

    gindex = 0;
    memset(gifr, 0, sizeof(gifr));
    gifc.ifc_len = sizeof(gifr);
    gifc.ifc_buf = (caddr_t)gifr;

    ret = ioctl(sockfd, SIOCGIFCONF, &gifc);
    close(sockfd);
    vlog((ret < 0), elog_ioctl);
    retE((ret < 0));

    req = &gifr[gindex];
    yes = !strncmp(req->ifr_name, "eth", 3) || !strncmp(req->ifr_name, "wlan", 4);
    while (!yes) {
        gindex++;
        if (gindex >= gifc.ifc_len/sizeof(struct ifreq)) {
            no_ifs = 1;
            break;
        }
        req = &gifr[gindex];
        yes = !strncmp(req->ifr_name, "eth", 3) || !strncmp(req->ifr_name, "wlan", 4);
    }
    retS((no_ifs));

    ip = inet_ntoa(((struct sockaddr_in*)&(req->ifr_addr))->sin_addr);
    retE((strlen(ip) + 1 > sz));
    strcpy(host, ip);
    gindex++;
    return 1;
}

int vhostaddr_get_next(char* host, int sz)
{
    struct ifreq* req = NULL;
    char* ip = NULL;
    int no_ifs = 0;
    int yes = 0;

    vassert(host);
    vassert(sz > 0);

    retE((!gindex));
    retS((gindex >= gifc.ifc_len/sizeof(struct ifreq)));

    req = &gifr[gindex];
    yes = !strncmp(req->ifr_name, "eth", 3) || !strncmp(req->ifr_name, "wlan", 4);
    while(!yes) {
        gindex++;
        if (gindex >= gifc.ifc_len/sizeof(struct ifreq)) {
            no_ifs = 1;
            break;
        }
        req = &gifr[gindex];
        yes = !strncmp(req->ifr_name, "eth", 3) || !strncmp(req->ifr_name, "wlan", 4);
    }
    retS((no_ifs));

    ip = inet_ntoa(((struct sockaddr_in*)&(req->ifr_addr))->sin_addr);
    retE((strlen(ip) + 1 > sz));
    strcpy(host, ip);
    gindex++;
    return 1;
}

int vsockaddr_get_by_hostname(const char* ip, const char* port, const char* proto, struct sockaddr_in* addr)
{
    int ipproto = 0;
    int scktype = 0;
    int ret = 0;

    vassert(ip);
    vassert(port);
    vassert(proto);
    vassert(addr);

    if (!strcmp(proto, "udp") || !strcmp(proto, "UDP")) {
        ipproto = IPPROTO_UDP;
        scktype = SOCK_DGRAM;
    } else if (!strcmp(proto, "tcp") || !strcmp(proto, "TCP")) {
        ipproto = IPPROTO_TCP;
        scktype = SOCK_STREAM;
    } else {
        retE((1));
    }

    {
        struct addrinfo hints;
        struct addrinfo* res = NULL;
        struct addrinfo* aip = NULL;

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = 0;
        hints.ai_protocol = ipproto;
        hints.ai_socktype = scktype;
        hints.ai_family = AF_UNSPEC;

        errno = 0;
        ret = getaddrinfo(ip, port, &hints, &res);
        vlog(((ret < 0) && (!errno)), printf("error:%s\n", gai_strerror(errno)));
        retE((ret < 0));
        for (aip = res; aip ; aip = aip->ai_next) {
            if (!aip->ai_addr) {
                continue;
            }
            vsockaddr_copy(addr, (struct sockaddr_in*)aip->ai_addr);
            break;
        }
        freeaddrinfo(res);
    }
    return 0;
}

int vsockaddr_within_same_network(struct sockaddr_in* a, struct sockaddr_in* b)
{
    vassert(a);
    vassert(b);

    if ((a->sin_family != b->sin_family)
        || ((uint32_t)a->sin_addr.s_addr != (uint32_t)b->sin_addr.s_addr)) {
        return 0;
    }
    return 1;
}

void vsockaddr_copy(struct sockaddr_in* dst, struct sockaddr_in* src)
{
    vassert(dst);
    vassert(src);

    memcpy(dst, src, sizeof(struct sockaddr_in));
    return ;
}

int vsockaddr_equal(struct sockaddr_in* a, struct sockaddr_in* b)
{
    vassert(a);
    vassert(b);

    if ((a->sin_family != b->sin_family)
      ||((uint32_t)a->sin_addr.s_addr != (uint32_t)b->sin_addr.s_addr)
      ||(a->sin_port != b->sin_port)){
        return 0;
    }
    return 1;
}

int vsockaddr_convert(const char* host, uint16_t port, struct sockaddr_in* addr)
{
    int ret = 0;
    vassert(host);
    vassert(port > 0);
    vassert(addr);

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    ret = inet_aton(host, (struct in_addr*)&addr->sin_addr);
    vlog((!ret), elog_inet_aton);
    retE((!ret));
    return 0;
}

int vsockaddr_convert2(uint32_t ip, uint16_t port, struct sockaddr_in* addr)
{
    vassert(addr);

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port   = htons(port);
    addr->sin_addr.s_addr = htonl(ip);

    return 0;
}

int vsockaddr_unconvert(struct sockaddr_in* addr, char* host, int len, uint16_t* port)
{
    char* hostname = NULL;

    vassert(addr);
    vassert(host);
    vassert(len > 0);
    vassert(port);

    hostname = inet_ntoa((struct in_addr)addr->sin_addr);
    vlog((!hostname), elog_inet_ntoa);
    retE((!hostname));
    retE((strlen(hostname) >= len));
    strcpy(host, hostname);

    *port = (int)ntohs(addr->sin_port);
    return 0;
}

int vsockaddr_unconvert2(struct sockaddr_in* addr, uint32_t* ip, uint16_t* port)
{
    vassert(addr);
    vassert(ip);
    vassert(port);

    *ip   = ntohl(addr->sin_addr.s_addr);
    *port = ntohs(addr->sin_port);

    return 0;
}

int vsockaddr_strlize(struct sockaddr_in* addr, char* buf, int len)
{
    uint16_t port = 0;
    int ret  = 0;
    int sz   = 0;

    vassert(addr);
    vassert(buf);
    vassert(len > 0);

    ret = vsockaddr_unconvert(addr, buf, len, &port);
    vlog((ret < 0), elog_vsockaddr_unconvert);
    retE((ret < 0));
    sz  = strlen(buf);
    ret = snprintf(buf + sz, len - sz, ":%d", port);
    vlog((ret >= len-sz), elog_snprintf);
    retE((ret >= len-sz));
    return 0;
}

int vsockaddr_unstrlize(const char* ip_addr, struct sockaddr_in* addr)
{
    char* s = NULL;
    char ip[64];
    int  port = 0;
    int  ret = 0;

    vassert(ip_addr);
    vassert(addr);

    s = strchr(ip_addr, ':');
    retE((!s));

    memset(ip, 0, 64);
    strncpy(ip, ip_addr, s - ip_addr);

    s += 1;
    errno = 0;
    port = strtol(s, NULL, 10);
    vlog((errno), elog_strtol);
    retE((errno));

    ret = vsockaddr_convert(ip, port, addr);
    vlog((ret < 0), elog_vsockaddr_convert);
    retE((ret < 0));
    return 0;
}

int vsockaddr_combine(struct sockaddr_in* ip_part, struct sockaddr_in* port_part, struct sockaddr_in* addr)
{
    vassert(ip_part);
    vassert(port_part);
    vassert(addr);

    memset(addr, 0, sizeof(*addr));

    addr->sin_family = AF_INET;
    addr->sin_port = port_part->sin_port;
    addr->sin_addr = ip_part->sin_addr;

    return 0;
}

static
int _aux_sockaddr_class(uint32_t haddr) //host byte order
{
    struct addr_class_pattern {
        int  klass;
        uint32_t mask;
    } patterns[] = {
        {VADDR_CLASS_A, 0x80000000 },
        {VADDR_CLASS_B, 0x40000000 },
        {VADDR_CLASS_C, 0x20000000 },
        {VADDR_CLASS_D, 0x10000000 },
        {VADDR_CLASS_E, 0 }
    };
    struct addr_class_pattern* pattern = patterns;

    for (; pattern->klass != VADDR_CLASS_E; pattern++) {
        if (!(haddr & pattern->mask)) {
            break;
        }
    }
    return pattern->klass;
}

int vsockaddr_is_public(struct sockaddr_in* addr)
{
    vassert(addr);

    //todo;
    return !vsockaddr_is_private(addr);
}

int vsockaddr_is_private(struct sockaddr_in* addr)
{
    struct priv_addr_pattern {
        int klass;
        uint32_t mask;
        uint32_t pattern;
    } priv_patterns[] = {
        {VADDR_CLASS_A, 0xff000000, 0x0a000000 }, //"10.0.0.0/8"
        {VADDR_CLASS_B, 0xfff00000, 0xac100000 }, //"172.16.0.0/12",
        {VADDR_CLASS_C, 0xffff0000, 0xc0a80000 }, //"192.168.0.0/16",
        {-1, 0, 0 }
    };
    struct priv_addr_pattern* pattern = priv_patterns;
    uint32_t haddr = ntohl(addr->sin_addr.s_addr);
    int klass = 0;
    int yes = 0;
    vassert(addr);

    klass = _aux_sockaddr_class(haddr);

    for (; pattern->klass != -1; pattern++) {
        if ((klass == pattern->klass)
           && ((haddr & pattern->mask) == pattern->pattern)){
            yes = 1;
            break;
        }
    }
    return yes;
}

int vsockaddr_dump(struct sockaddr_in* addr)
{
    uint16_t port = 0;
    char ip[64];
    int ret  = 0;
    vassert(addr);

    memset(ip, 0, 64);
    ret = vsockaddr_unconvert(addr, ip, 64, &port);
    vlog((ret < 0), elog_vsockaddr_unconvert);
    retE((ret < 0));
    printf("%s:%d", ip, port);
    return 0;
}

int vmacaddr_get(uint8_t* macaddr, int len)
{
    struct ifreq req;
    int ret = 0;
    int fd  = 0;

    vassert(macaddr);
    vassert(len > 0);
    retE((len < ETH_ALEN)); // ETH_ALEN (6).

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    vlog((fd < 0), elog_socket);
    retE((fd < 0));

    strcpy(req.ifr_name, "eth0");
    ret = ioctl(fd, SIOCGIFHWADDR, &req);
    close(fd);
    vlog((ret < 0), elog_ioctl);
    retE((ret < 0));

    memcpy(macaddr, req.ifr_hwaddr.sa_data, ETH_ALEN);
    return 0;
}

