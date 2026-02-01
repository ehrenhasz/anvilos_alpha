 

#include "includes.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include "addr.h"
#include "canohost.h"
#include "log.h"
#include "misc.h"
#include "srclimit.h"
#include "xmalloc.h"

static int max_children, max_persource, ipv4_masklen, ipv6_masklen;

 
static struct child_info {
	int id;
	struct xaddr addr;
} *child;

void
srclimit_init(int max, int persource, int ipv4len, int ipv6len)
{
	int i;

	max_children = max;
	ipv4_masklen = ipv4len;
	ipv6_masklen = ipv6len;
	max_persource = persource;
	if (max_persource == INT_MAX)	 
		return;
	debug("%s: max connections %d, per source %d, masks %d,%d", __func__,
	    max, persource, ipv4len, ipv6len);
	if (max <= 0)
		fatal("%s: invalid number of sockets: %d", __func__, max);
	child = xcalloc(max_children, sizeof(*child));
	for (i = 0; i < max_children; i++)
		child[i].id = -1;
}

 
int
srclimit_check_allow(int sock, int id)
{
	struct xaddr xa, xb, xmask;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	struct sockaddr *sa = (struct sockaddr *)&addr;
	int i, bits, first_unused, count = 0;
	char xas[NI_MAXHOST];

	if (max_persource == INT_MAX)	 
		return 1;

	debug("%s: sock %d id %d limit %d", __func__, sock, id, max_persource);
	if (getpeername(sock, sa, &addrlen) != 0)
		return 1;	 
	if (addr_sa_to_xaddr(sa, addrlen, &xa) != 0)
		return 1;	 

	 
	bits = xa.af == AF_INET ? ipv4_masklen : ipv6_masklen;
	if (addr_netmask(xa.af, bits, &xmask) != 0 ||
	    addr_and(&xb, &xa, &xmask) != 0) {
		debug3("%s: invalid mask %d bits", __func__, bits);
		return 1;
	}

	first_unused = max_children;
	 
	for (i = 0; i < max_children; i++) {
		if (child[i].id == -1) {
			if (i < first_unused)
				first_unused = i;
		} else if (addr_cmp(&child[i].addr, &xb) == 0) {
			count++;
		}
	}
	if (addr_ntop(&xa, xas, sizeof(xas)) != 0) {
		debug3("%s: addr ntop failed", __func__);
		return 1;
	}
	debug3("%s: new unauthenticated connection from %s/%d, at %d of %d",
	    __func__, xas, bits, count, max_persource);

	if (first_unused == max_children) {  
		debug3("%s: no free slot", __func__);
		return 0;
	}
	if (first_unused < 0 || first_unused >= max_children)
		fatal("%s: internal error: first_unused out of range",
		    __func__);

	if (count >= max_persource)
		return 0;

	 
	child[first_unused].id = id;
	memcpy(&child[first_unused].addr, &xb, sizeof(xb));
	return 1;
}

void
srclimit_done(int id)
{
	int i;

	if (max_persource == INT_MAX)	 
		return;

	debug("%s: id %d", __func__, id);
	 
	for (i = 0; i < max_children; i++) {
		if (child[i].id == id) {
			child[i].id = -1;
			return;
		}
	}
}
