
#ifndef IP_COMMON_H
#define IP_COMMON_H 1

#include "libbb.h"
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#if !defined IFA_RTA
#include <linux/if_addr.h>
#endif
#if !defined IFLA_RTA
#include <linux/if_link.h>
#endif

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

char FAST_FUNC **ip_parse_common_args(char **argv);

int FAST_FUNC ipaddr_list_or_flush(char **argv, int flush);



int FAST_FUNC do_ipaddr(char **argv);
int FAST_FUNC do_iproute(char **argv);
int FAST_FUNC do_iprule(char **argv);
int FAST_FUNC do_ipneigh(char **argv);
int FAST_FUNC do_iptunnel(char **argv);
int FAST_FUNC do_iplink(char **argv);




POP_SAVED_FUNCTION_VISIBILITY

#ifndef	INFINITY_LIFE_TIME
#define     INFINITY_LIFE_TIME      0xFFFFFFFFU
#endif

#endif
