 

 

 

 

#include "includes.h"

#ifndef HAVE_BINDRESVPORT_SA
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define STARTPORT 600
#define ENDPORT (IPPORT_RESERVED - 1)
#define NPORTS	(ENDPORT - STARTPORT + 1)

 
int
bindresvport_sa(int sd, struct sockaddr *sa)
{
	int error, af;
	struct sockaddr_storage myaddr;
	struct sockaddr_in *in;
	struct sockaddr_in6 *in6;
	u_int16_t *portp;
	u_int16_t port;
	socklen_t salen;
	int i;

	if (sa == NULL) {
		memset(&myaddr, 0, sizeof(myaddr));
		sa = (struct sockaddr *)&myaddr;
		salen = sizeof(myaddr);

		if (getsockname(sd, sa, &salen) == -1)
			return -1;	 

		af = sa->sa_family;
		memset(&myaddr, 0, salen);
	} else
		af = sa->sa_family;

	if (af == AF_INET) {
		in = (struct sockaddr_in *)sa;
		salen = sizeof(struct sockaddr_in);
		portp = &in->sin_port;
	} else if (af == AF_INET6) {
		in6 = (struct sockaddr_in6 *)sa;
		salen = sizeof(struct sockaddr_in6);
		portp = &in6->sin6_port;
	} else {
		errno = EPFNOSUPPORT;
		return (-1);
	}
	sa->sa_family = af;

	port = ntohs(*portp);
	if (port == 0)
		port = arc4random_uniform(NPORTS) + STARTPORT;

	 
	error = -1;

	for(i = 0; i < NPORTS; i++) {
		*portp = htons(port);
		
		error = bind(sd, sa, salen);

		 
		if (error == 0)
			break;
			
		 
		if ((error < 0) && !((errno == EADDRINUSE) || (errno == EINVAL)))
			break;
			
		port++;
		if (port > ENDPORT)
			port = STARTPORT;
	}

	return (error);
}

#endif  
