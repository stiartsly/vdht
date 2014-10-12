#include "vglobal.h"
#include "vmisc.h"

/**
 * @ host: [in, out]
 * @ len:  [in] buffer size;
 * @ port: [out]
 **/
int vhostaddr_get(char* host, int sz, int* port)
{
    vassert(host);
    vassert(sz > 0);
    vassert(port);
    //todo;
    return 0;
}

int vsockaddr_get(struct sockaddr_in* addr)
{
    vassert(addr);

    //todo;
    return 0;
}

int vsockaddr_copy(struct sockaddr_in* dst, struct sockaddr_in* src)
{
    vassert(dst);
    vassert(src);

    memcpy(dst, src, sizeof(struct sockaddr_in));
    return 0;
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
    ret1E((!ret), elog_inet_aton);
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
    ret1E((!hostname), elog_inet_ntoa);
    retE((strlen(hostname) >= len));

    *port = (int)ntohs(addr->sin_port);
    return 0;
}

int vsockaddr_equal(struct sockaddr_in* a, struct sockaddr_in* b)
{
    vassert(a);
    vassert(b);
    //todo;
    return 0;
}

