 

 

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "addr.h"
#include "match.h"
#include "log.h"

 
int
addr_match_list(const char *addr, const char *_list)
{
	char *list, *cp, *o;
	struct xaddr try_addr, match_addr;
	u_int masklen, neg;
	int ret = 0, r;

	if (addr != NULL && addr_pton(addr, &try_addr) != 0) {
		debug2_f("couldn't parse address %.100s", addr);
		return 0;
	}
	if ((o = list = strdup(_list)) == NULL)
		return -1;
	while ((cp = strsep(&list, ",")) != NULL) {
		neg = *cp == '!';
		if (neg)
			cp++;
		if (*cp == '\0') {
			ret = -2;
			break;
		}
		 
		r = addr_pton_cidr(cp, &match_addr, &masklen);
		if (r == -2) {
			debug2_f("inconsistent mask length for "
			    "match network \"%.100s\"", cp);
			ret = -2;
			break;
		} else if (r == 0) {
			if (addr != NULL && addr_netmatch(&try_addr,
			    &match_addr, masklen) == 0) {
 foundit:
				if (neg) {
					ret = -1;
					break;
				}
				ret = 1;
			}
			continue;
		} else {
			 
			if (addr != NULL && match_pattern(addr, cp) == 1)
				goto foundit;
		}
	}
	free(o);

	return ret;
}

 
int
addr_match_cidr_list(const char *addr, const char *_list)
{
	char *list, *cp, *o;
	struct xaddr try_addr, match_addr;
	u_int masklen;
	int ret = 0, r;

	if (addr != NULL && addr_pton(addr, &try_addr) != 0) {
		debug2_f("couldn't parse address %.100s", addr);
		return 0;
	}
	if ((o = list = strdup(_list)) == NULL)
		return -1;
	while ((cp = strsep(&list, ",")) != NULL) {
		if (*cp == '\0') {
			error_f("empty entry in list \"%.100s\"", o);
			ret = -1;
			break;
		}

		 

		 
		if (strlen(cp) > INET6_ADDRSTRLEN + 3) {
			error_f("list entry \"%.100s\" too long", cp);
			ret = -1;
			break;
		}
#define VALID_CIDR_CHARS "0123456789abcdefABCDEF.:/"
		if (strspn(cp, VALID_CIDR_CHARS) != strlen(cp)) {
			error_f("list entry \"%.100s\" contains invalid "
			    "characters", cp);
			ret = -1;
		}

		 
		r = addr_pton_cidr(cp, &match_addr, &masklen);
		if (r == -1) {
			error("Invalid network entry \"%.100s\"", cp);
			ret = -1;
			break;
		} else if (r == -2) {
			error("Inconsistent mask length for "
			    "network \"%.100s\"", cp);
			ret = -1;
			break;
		} else if (r == 0 && addr != NULL) {
			if (addr_netmatch(&try_addr, &match_addr,
			    masklen) == 0)
				ret = 1;
			continue;
		}
	}
	free(o);

	return ret;
}
