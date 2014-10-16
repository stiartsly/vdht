#include "vglobal.h"
#include "vmisc.h"

/**
 * @ host: [in, out]
 * @ len:  [in] buffer size;
 * @ port: [out]
 **/
static struct hostent* ghent = NULL;
static int gindex = 0;
int vhostaddr_get_first(char* host, int sz)
{
    char hname[128];
    char* tmp = NULL;
    int ret = 0;

    vassert(host);
    vassert(sz > 0);

    ret = gethostname(hname, 128);
    vlog((ret < 0), elog_gethostname);
    retE((ret < 0));

    gindex = 0;
    ghent  = gethostbyname(hname);
    vlog((!ghent), elog_gethostbyname);
    retE((!ghent));

    tmp = inet_ntoa(*(struct in_addr*)(ghent->h_addr_list[gindex]));
    vlog((!tmp), elog_inet_ntoa);
    retE((!tmp));
    retE((strlen(tmp) + 1 > sz));
    strcpy(host, tmp);

    gindex++;
    return 0;
}

int vhostaddr_get_next(char* host, int sz)
{
    char* tmp = NULL;
    vassert(host);
    vassert(sz > 0);

    retE((!ghent));
    retE((!gindex >= 1));

    tmp = inet_ntoa(*(struct in_addr*)(ghent->h_addr_list[gindex]));
    vlog((!tmp), elog_inet_ntoa);
    retE((!tmp));
    retE((strlen(tmp) + 1 > sz));
    strcpy(host, tmp);

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

int vsockaddr_convert(const char* host, int port, struct sockaddr_in* addr)
{
    int ret = 0;
    vassert(host);
    vassert(port > 0);
    vassert(addr);

    addr->sin_family = AF_INET;
    addr->sin_port = htons((unsigned short)port);
    memset(&addr->sin_zero, 0, 8);

    ret = inet_aton(host, (struct in_addr*)&addr->sin_addr);
    vlog((!ret), elog_inet_aton);
    retE((!ret));
    return 0;
}

int vsockaddr_unconvert(struct sockaddr_in* addr, char* host, int len, int* port)
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

