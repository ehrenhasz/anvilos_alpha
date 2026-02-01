 

#include "core.h"
#include "bearer.h"

 
static int tipc_eth_addr2str(struct tipc_media_addr *addr,
			     char *strbuf, int bufsz)
{
	if (bufsz < 18)	 
		return 1;

	sprintf(strbuf, "%pM", addr->value);
	return 0;
}

 
static int tipc_eth_addr2msg(char *msg, struct tipc_media_addr *addr)
{
	memset(msg, 0, TIPC_MEDIA_INFO_SIZE);
	msg[TIPC_MEDIA_TYPE_OFFSET] = TIPC_MEDIA_TYPE_ETH;
	memcpy(msg + TIPC_MEDIA_ADDR_OFFSET, addr->value, ETH_ALEN);
	return 0;
}

 
static int tipc_eth_raw2addr(struct tipc_bearer *b,
			     struct tipc_media_addr *addr,
			     const char *msg)
{
	memset(addr, 0, sizeof(*addr));
	ether_addr_copy(addr->value, msg);
	addr->media_id = TIPC_MEDIA_TYPE_ETH;
	addr->broadcast = is_broadcast_ether_addr(addr->value);
	return 0;
}

 
static int tipc_eth_msg2addr(struct tipc_bearer *b,
			     struct tipc_media_addr *addr,
			     char *msg)
{
	 
	msg += TIPC_MEDIA_ADDR_OFFSET;
	return tipc_eth_raw2addr(b, addr, msg);
}

 
struct tipc_media eth_media_info = {
	.send_msg	= tipc_l2_send_msg,
	.enable_media	= tipc_enable_l2_media,
	.disable_media	= tipc_disable_l2_media,
	.addr2str	= tipc_eth_addr2str,
	.addr2msg	= tipc_eth_addr2msg,
	.msg2addr	= tipc_eth_msg2addr,
	.raw2addr	= tipc_eth_raw2addr,
	.priority	= TIPC_DEF_LINK_PRI,
	.tolerance	= TIPC_DEF_LINK_TOL,
	.min_win	= TIPC_DEF_LINK_WIN,
	.max_win	= TIPC_MAX_LINK_WIN,
	.type_id	= TIPC_MEDIA_TYPE_ETH,
	.hwaddr_len	= ETH_ALEN,
	.name		= "eth"
};
