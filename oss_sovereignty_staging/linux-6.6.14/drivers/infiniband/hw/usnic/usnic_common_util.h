 

#ifndef USNIC_CMN_UTIL_H
#define USNIC_CMN_UTIL_H

#include <net/addrconf.h>

static inline void
usnic_mac_ip_to_gid(const char *const mac, const __be32 inaddr, char *raw_gid)
{
	raw_gid[0] = 0xfe;
	raw_gid[1] = 0x80;
	memset(&raw_gid[2], 0, 2);
	memcpy(&raw_gid[4], &inaddr, 4);
	addrconf_addr_eui48(&raw_gid[8], mac);
}

#endif  
