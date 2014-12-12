#include "vglobal.h"
#include "vmisc.h"

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
    char* ip = NULL;
    int sockfd = 0;
    int ret = 0;

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

    ip = inet_ntoa(((struct sockaddr_in*)&(gifr[gindex].ifr_addr))->sin_addr);
    if (strcmp(ip, "127.0.0.1")) {
        retE((strlen(ip) + 1 > sz));
        strcpy(host, ip);
        return 0;
    }
    gindex++;

    retE((gindex >= gifc.ifc_len/sizeof(struct ifreq)));
    ip = inet_ntoa(((struct sockaddr_in*)&(gifr[gindex].ifr_addr))->sin_addr);
    retE((strlen(ip) + 1 > sz));
    strcpy(host, ip);
    gindex++;

    return 0;
}

int vhostaddr_get_next(char* host, int sz)
{
    char* ip = NULL;
    vassert(host);
    vassert(sz > 0);

    retE((!gindex));
    retE((gindex >= gifc.ifc_len/sizeof(struct ifreq)));

    ip = inet_ntoa(((struct sockaddr_in*)&(gifr[gindex].ifr_addr))->sin_addr);
    if (strcmp(ip, "127.0.0.1")) {
        retE((strlen(ip) + 1 > sz));
        strcpy(host, ip);
        return 0;
    }
    gindex++;

    retE((gindex >= gifc.ifc_len/sizeof(struct ifreq)));
    ip = inet_ntoa(((struct sockaddr_in*)&(gifr[gindex].ifr_addr))->sin_addr);
    retE((strlen(ip) + 1 > sz));
    strcpy(host, ip);
    gindex++;

    return 0;
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
    vdump(printf("ADDR:%s:%d", ip, port));
    return 0;
}

