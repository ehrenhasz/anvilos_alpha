
 

#include <linux/types.h>
#include <linux/ipv6.h>

#include <net/sock.h>
#include <net/inet_sock.h>
#include <net/netlink.h>
#include <net/genetlink.h>

#include "handshake.h"

#define CREATE_TRACE_POINTS

#include <trace/events/handshake.h>
