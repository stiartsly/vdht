#ifndef __VMISC_H__
#define __VMISC_H__

#include <netinet/in.h>
/*
 * for host address.
 */
int vhostaddr_get_fist(char*, int);
int vhostaddr_get_next(char*, int);

/*
 * for sockaddr
 */

void vsockaddr_copy (struct sockaddr_in*, struct sockaddr_in*);
int  vsockaddr_equal(struct sockaddr_in*, struct sockaddr_in*);
int  vsockaddr_convert(const char*, int, struct sockaddr_in*);
int  vsockaddr_unconvert(struct sockaddr_in*, char*, int, int*);

#endif
