#ifndef __VMISC_H__
#define __VMISC_H__

#include <netinet/in.h>
/*
 * for host address.
 */
int vhostaddr_get(char*, int, int*);

/*
 * for sockaddr
 */

#define VSOCKADDR(addr) \
    { \
        .sin_addr = addr->sin_addr,    \
        .sin_port = addr->port,        \
        .sin_family = addr->sin_family, \
        .sin_zero = {0,} \
    }

int vsockaddr_get  (struct sockaddr_in*);
int vsockaddr_copy (struct sockaddr_in*, struct sockaddr_in*);
int vsockaddr_equal(struct sockaddr_in*, struct sockaddr_in*);
int vsockaddr_convert(const char*, int, struct sockaddr_in*);
int vsockaddr_unconvert(struct sockaddr_in*, char*, int, int*);

#endif
