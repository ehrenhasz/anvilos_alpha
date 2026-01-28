


#ifndef EF4_FILTER_H
#define EF4_FILTER_H

#include <linux/types.h>
#include <linux/if_ether.h>
#include <asm/byteorder.h>


enum ef4_filter_match_flags {
	EF4_FILTER_MATCH_REM_HOST =	0x0001,
	EF4_FILTER_MATCH_LOC_HOST =	0x0002,
	EF4_FILTER_MATCH_REM_MAC =	0x0004,
	EF4_FILTER_MATCH_REM_PORT =	0x0008,
	EF4_FILTER_MATCH_LOC_MAC =	0x0010,
	EF4_FILTER_MATCH_LOC_PORT =	0x0020,
	EF4_FILTER_MATCH_ETHER_TYPE =	0x0040,
	EF4_FILTER_MATCH_INNER_VID =	0x0080,
	EF4_FILTER_MATCH_OUTER_VID =	0x0100,
	EF4_FILTER_MATCH_IP_PROTO =	0x0200,
	EF4_FILTER_MATCH_LOC_MAC_IG =	0x0400,
};


enum ef4_filter_priority {
	EF4_FILTER_PRI_HINT = 0,
	EF4_FILTER_PRI_AUTO,
	EF4_FILTER_PRI_MANUAL,
	EF4_FILTER_PRI_REQUIRED,
};


enum ef4_filter_flags {
	EF4_FILTER_FLAG_RX_RSS = 0x01,
	EF4_FILTER_FLAG_RX_SCATTER = 0x02,
	EF4_FILTER_FLAG_RX_OVER_AUTO = 0x04,
	EF4_FILTER_FLAG_RX = 0x08,
	EF4_FILTER_FLAG_TX = 0x10,
};


struct ef4_filter_spec {
	u32	match_flags:12;
	u32	priority:2;
	u32	flags:6;
	u32	dmaq_id:12;
	u32	rss_context;
	__be16	outer_vid __aligned(4); 
	__be16	inner_vid;
	u8	loc_mac[ETH_ALEN];
	u8	rem_mac[ETH_ALEN];
	__be16	ether_type;
	u8	ip_proto;
	__be32	loc_host[4];
	__be32	rem_host[4];
	__be16	loc_port;
	__be16	rem_port;
	
};

enum {
	EF4_FILTER_RSS_CONTEXT_DEFAULT = 0xffffffff,
	EF4_FILTER_RX_DMAQ_ID_DROP = 0xfff
};

static inline void ef4_filter_init_rx(struct ef4_filter_spec *spec,
				      enum ef4_filter_priority priority,
				      enum ef4_filter_flags flags,
				      unsigned rxq_id)
{
	memset(spec, 0, sizeof(*spec));
	spec->priority = priority;
	spec->flags = EF4_FILTER_FLAG_RX | flags;
	spec->rss_context = EF4_FILTER_RSS_CONTEXT_DEFAULT;
	spec->dmaq_id = rxq_id;
}

static inline void ef4_filter_init_tx(struct ef4_filter_spec *spec,
				      unsigned txq_id)
{
	memset(spec, 0, sizeof(*spec));
	spec->priority = EF4_FILTER_PRI_REQUIRED;
	spec->flags = EF4_FILTER_FLAG_TX;
	spec->dmaq_id = txq_id;
}


static inline int
ef4_filter_set_ipv4_local(struct ef4_filter_spec *spec, u8 proto,
			  __be32 host, __be16 port)
{
	spec->match_flags |=
		EF4_FILTER_MATCH_ETHER_TYPE | EF4_FILTER_MATCH_IP_PROTO |
		EF4_FILTER_MATCH_LOC_HOST | EF4_FILTER_MATCH_LOC_PORT;
	spec->ether_type = htons(ETH_P_IP);
	spec->ip_proto = proto;
	spec->loc_host[0] = host;
	spec->loc_port = port;
	return 0;
}


static inline int
ef4_filter_set_ipv4_full(struct ef4_filter_spec *spec, u8 proto,
			 __be32 lhost, __be16 lport,
			 __be32 rhost, __be16 rport)
{
	spec->match_flags |=
		EF4_FILTER_MATCH_ETHER_TYPE | EF4_FILTER_MATCH_IP_PROTO |
		EF4_FILTER_MATCH_LOC_HOST | EF4_FILTER_MATCH_LOC_PORT |
		EF4_FILTER_MATCH_REM_HOST | EF4_FILTER_MATCH_REM_PORT;
	spec->ether_type = htons(ETH_P_IP);
	spec->ip_proto = proto;
	spec->loc_host[0] = lhost;
	spec->loc_port = lport;
	spec->rem_host[0] = rhost;
	spec->rem_port = rport;
	return 0;
}

enum {
	EF4_FILTER_VID_UNSPEC = 0xffff,
};


static inline int ef4_filter_set_eth_local(struct ef4_filter_spec *spec,
					   u16 vid, const u8 *addr)
{
	if (vid == EF4_FILTER_VID_UNSPEC && addr == NULL)
		return -EINVAL;

	if (vid != EF4_FILTER_VID_UNSPEC) {
		spec->match_flags |= EF4_FILTER_MATCH_OUTER_VID;
		spec->outer_vid = htons(vid);
	}
	if (addr != NULL) {
		spec->match_flags |= EF4_FILTER_MATCH_LOC_MAC;
		ether_addr_copy(spec->loc_mac, addr);
	}
	return 0;
}


static inline int ef4_filter_set_uc_def(struct ef4_filter_spec *spec)
{
	spec->match_flags |= EF4_FILTER_MATCH_LOC_MAC_IG;
	return 0;
}


static inline int ef4_filter_set_mc_def(struct ef4_filter_spec *spec)
{
	spec->match_flags |= EF4_FILTER_MATCH_LOC_MAC_IG;
	spec->loc_mac[0] = 1;
	return 0;
}

#endif 
