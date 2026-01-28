#ifndef _DCCP_IPV6_H
#define _DCCP_IPV6_H
#include <linux/dccp.h>
#include <linux/ipv6.h>
struct dccp6_sock {
	struct dccp_sock  dccp;
	struct ipv6_pinfo inet6;
};
struct dccp6_request_sock {
	struct dccp_request_sock  dccp;
};
struct dccp6_timewait_sock {
	struct inet_timewait_sock   inet;
};
#endif  
