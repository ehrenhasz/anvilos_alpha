 
 

 

#include "includes.h"

#if defined(BROKEN_INET_NTOA) || !defined(HAVE_INET_NTOA)

 
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

char *
inet_ntoa(struct in_addr in)
{
	static char b[18];
	char *p;

	p = (char *)&in;
#define	UC(b)	(((int)b)&0xff)
	(void)snprintf(b, sizeof(b),
	    "%u.%u.%u.%u", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	return (b);
}

#endif  
