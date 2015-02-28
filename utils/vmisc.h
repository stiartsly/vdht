#ifndef __VMISC_H__
#define __VMISC_H__

#include <netinet/in.h>
/*
 * for host address.
 */
int vhostaddr_get_first(char*, int);
int vhostaddr_get_next (char*, int);

/*
 * for sockaddr
 */

int  vsockaddr_get_by_hostname(const char*, const char*, const char*, struct sockaddr_in*);
int  vsockaddr_within_same_network(struct sockaddr_in*, struct sockaddr_in*);
void vsockaddr_copy      (struct sockaddr_in*, struct sockaddr_in*);
int  vsockaddr_equal     (struct sockaddr_in*, struct sockaddr_in*);
int  vsockaddr_convert   (const char*, uint16_t, struct sockaddr_in*);
int  vsockaddr_convert2  (uint32_t, uint16_t, struct sockaddr_in*);
int  vsockaddr_unconvert (struct sockaddr_in*, char*, int, uint16_t*);
int  vsockaddr_unconvert2(struct sockaddr_in*, uint32_t*, uint16_t*);
int  vsockaddr_strlize   (struct sockaddr_in*, char*, int );
int  vsockaddr_unstrlize (const char*, struct sockaddr_in*);
int  vsockaddr_is_public (struct sockaddr_in*);
int  vsockaddr_is_private(struct sockaddr_in*);
int  vsockaddr_dump      (struct sockaddr_in*);

/*
 * for macaddr
 */
int  vmacaddr_get(uint8_t*, int);

#endif

