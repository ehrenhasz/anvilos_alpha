 

#include <linux/if_infiniband.h>
#include "core.h"
#include "bearer.h"

#define TIPC_MAX_IB_LINK_WIN 500

 
static int tipc_ib_addr2str(struct tipc_media_addr *a, char *str_buf,
			    int str_size)
{
	if (str_size < 60)	 
		return 1;

	sprintf(str_buf, "%20phC", a->value);

	return 0;
}

 
static int tipc_ib_addr2msg(char *msg, struct tipc_media_addr *addr)
{
	memset(msg, 0, TIPC_MEDIA_INFO_SIZE);
	memcpy(msg, addr->value, INFINIBAND_ALEN);
	return 0;
}

 
static int tipc_ib_raw2addr(struct tipc_bearer *b,
			    struct tipc_media_addr *addr,
			    const char *msg)
{
	memset(addr, 0, sizeof(*addr));
	memcpy(addr->value, msg, INFINIBAND_ALEN);
	addr->media_id = TIPC_MEDIA_TYPE_IB;
	addr->broadcast = !memcmp(msg, b->bcast_addr.value,
				  INFINIBAND_ALEN);
	return 0;
}

 
static int tipc_ib_msg2addr(struct tipc_bearer *b,
			    struct tipc_media_addr *addr,
			    char *msg)
{
	return tipc_ib_raw2addr(b, addr, msg);
}

 
struct tipc_media ib_media_info = {
	.send_msg	= tipc_l2_send_msg,
	.enable_media	= tipc_enable_l2_media,
	.disable_media	= tipc_disable_l2_media,
	.addr2str	= tipc_ib_addr2str,
	.addr2msg	= tipc_ib_addr2msg,
	.msg2addr	= tipc_ib_msg2addr,
	.raw2addr	= tipc_ib_raw2addr,
	.priority	= TIPC_DEF_LINK_PRI,
	.tolerance	= TIPC_DEF_LINK_TOL,
	.min_win	= TIPC_DEF_LINK_WIN,
	.max_win	= TIPC_MAX_IB_LINK_WIN,
	.type_id	= TIPC_MEDIA_TYPE_IB,
	.hwaddr_len	= INFINIBAND_ALEN,
	.name		= "ib"
};
