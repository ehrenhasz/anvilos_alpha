 
 

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "xmalloc.h"
#include "packet.h"
#include "log.h"
#include "canohost.h"
#include "misc.h"

void
ipv64_normalise_mapped(struct sockaddr_storage *addr, socklen_t *len)
{
	struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
	struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
	struct in_addr inaddr;
	u_int16_t port;

	if (addr->ss_family != AF_INET6 ||
	    !IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr))
		return;

	debug3("Normalising mapped IPv4 in IPv6 address");

	memcpy(&inaddr, ((char *)&a6->sin6_addr) + 12, sizeof(inaddr));
	port = a6->sin6_port;

	memset(a4, 0, sizeof(*a4));

	a4->sin_family = AF_INET;
	*len = sizeof(*a4);
	memcpy(&a4->sin_addr, &inaddr, sizeof(inaddr));
	a4->sin_port = port;
}

 
static char *
get_socket_address(int sock, int remote, int flags)
{
	struct sockaddr_storage addr;
	socklen_t addrlen;
	char ntop[NI_MAXHOST];
	int r;

	if (sock < 0)
		return NULL;

	 
	addrlen = sizeof(addr);
	memset(&addr, 0, sizeof(addr));

	if (remote) {
		if (getpeername(sock, (struct sockaddr *)&addr, &addrlen) != 0)
			return NULL;
	} else {
		if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) != 0)
			return NULL;
	}

	 
	if (addr.ss_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
		ipv64_normalise_mapped(&addr, &addrlen);
	}

	switch (addr.ss_family) {
	case AF_INET:
	case AF_INET6:
		 
		if ((r = getnameinfo((struct sockaddr *)&addr, addrlen, ntop,
		    sizeof(ntop), NULL, 0, flags)) != 0) {
			error_f("getnameinfo %d failed: %s",
			    flags, ssh_gai_strerror(r));
			return NULL;
		}
		return xstrdup(ntop);
	case AF_UNIX:
		 
		return xstrdup(((struct sockaddr_un *)&addr)->sun_path);
	default:
		 
		return NULL;
	}
}

char *
get_peer_ipaddr(int sock)
{
	char *p;

	if ((p = get_socket_address(sock, 1, NI_NUMERICHOST)) != NULL)
		return p;
	return xstrdup("UNKNOWN");
}

char *
get_local_ipaddr(int sock)
{
	char *p;

	if ((p = get_socket_address(sock, 0, NI_NUMERICHOST)) != NULL)
		return p;
	return xstrdup("UNKNOWN");
}

char *
get_local_name(int fd)
{
	char *host, myname[NI_MAXHOST];

	 
	if ((host = get_socket_address(fd, 0, NI_NAMEREQD)) != NULL)
		return host;

	 
	if (gethostname(myname, sizeof(myname)) == -1) {
		verbose_f("gethostname: %s", strerror(errno));
		host = xstrdup("UNKNOWN");
	} else {
		host = xstrdup(myname);
	}

	return host;
}

 

static int
get_sock_port(int sock, int local)
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	char strport[NI_MAXSERV];
	int r;

	if (sock < 0)
		return -1;
	 
	fromlen = sizeof(from);
	memset(&from, 0, sizeof(from));
	if (local) {
		if (getsockname(sock, (struct sockaddr *)&from, &fromlen) == -1) {
			error("getsockname failed: %.100s", strerror(errno));
			return 0;
		}
	} else {
		if (getpeername(sock, (struct sockaddr *)&from, &fromlen) == -1) {
			debug("getpeername failed: %.100s", strerror(errno));
			return -1;
		}
	}

	 
	if (from.ss_family == AF_INET6)
		fromlen = sizeof(struct sockaddr_in6);

	 
	if (from.ss_family != AF_INET && from.ss_family != AF_INET6)
		return 0;

	 
	if ((r = getnameinfo((struct sockaddr *)&from, fromlen, NULL, 0,
	    strport, sizeof(strport), NI_NUMERICSERV)) != 0)
		fatal_f("getnameinfo NI_NUMERICSERV failed: %s",
		    ssh_gai_strerror(r));
	return atoi(strport);
}

int
get_peer_port(int sock)
{
	return get_sock_port(sock, 0);
}

int
get_local_port(int sock)
{
	return get_sock_port(sock, 1);
}
