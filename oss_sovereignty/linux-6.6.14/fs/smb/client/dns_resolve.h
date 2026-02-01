 
 

#ifndef _DNS_RESOLVE_H
#define _DNS_RESOLVE_H

#include <linux/net.h>

#ifdef __KERNEL__
int dns_resolve_server_name_to_ip(const char *unc, struct sockaddr *ip_addr, time64_t *expiry);
#endif  

#endif  
