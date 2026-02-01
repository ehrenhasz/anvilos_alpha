 

 

 

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)inet_addr.c	8.1 (Berkeley) 6/17/93";
static char rcsid[] = "$Id: inet_addr.c,v 1.5 1996/08/14 03:48:37 drepper Exp $";
#endif  

#include <config.h>

#if !defined (HAVE_INET_ATON) && defined (HAVE_NETWORK) && defined (HAVE_NETINET_IN_H) && defined (HAVE_ARPA_INET_H)

#include <sys/types.h>
#if defined (HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <bashansi.h>
#include <ctype.h>
#include <stdc.h>

#ifndef INADDR_NONE
#  define INADDR_NONE 0xffffffff
#endif

 

#if 0
 
 
u_long
inet_addr(cp)
	register const char *cp;
{
	struct in_addr val;

	if (inet_aton(cp, &val))
		return (val.s_addr);
	return (INADDR_NONE);
}
#endif

 
int
inet_aton(cp, addr)
	register const char *cp;
	struct in_addr *addr;
{
	register u_bits32_t val;
	register int base, n;
	register unsigned char c;
	u_int parts[4];
	register u_int *pp = parts;

	c = *cp;
	for (;;) {
		 
#if 0
		if (!isdigit(c))
#else
		if (c != '0' && c != '1' && c != '2' && c != '3' && c != '4' &&
		    c != '5' && c != '6' && c != '7' && c != '8' && c != '9')
#endif
			return (0);
		val = 0; base = 10;
		if (c == '0') {
			c = *++cp;
			if (c == 'x' || c == 'X')
				base = 16, c = *++cp;
			else
				base = 8;
		}
		for (;;) {
			if (isascii(c) && isdigit(c)) {
				val = (val * base) + (c - '0');
				c = *++cp;
			} else if (base == 16 && isascii(c) && isxdigit(c)) {
				val = (val << 4) |
					(c + 10 - (islower(c) ? 'a' : 'A'));
				c = *++cp;
			} else
				break;
		}
		if (c == '.') {
			 
			if (pp >= parts + 3)
				return (0);
			*pp++ = val;
			c = *++cp;
		} else
			break;
	}
	 
	if (c != '\0' && (!isascii(c) || !isspace(c)))
		return (0);
	 
	n = pp - parts + 1;
	switch (n) {

	case 0:
		return (0);		 

	case 1:				 
		break;

	case 2:				 
		if (val > 0xffffff)
			return (0);
		val |= parts[0] << 24;
		break;

	case 3:				 
		if (val > 0xffff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 4:				 
		if (val > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}
	if (addr)
		addr->s_addr = htonl(val);
	return (1);
}

#endif  
