


#ifndef EFX_FILTER_H
#define EFX_FILTER_H

#include <linux/types.h>
#include <linux/if_ether.h>
#include <asm/byteorder.h>


enum efx_filter_match_flags {
	EFX_FILTER_MATCH_REM_HOST =	0x0001,
	EFX_FILTER_MATCH_LOC_HOST =	0x0002,
	EFX_FILTER_MATCH_REM_MAC =	0x0004,
	EFX_FILTER_MATCH_REM_PORT =	0x0008,
	EFX_FILTER_MATCH_LOC_MAC =	0x0010,
	EFX_FILTER_MATCH_LOC_PORT =	0x0020,
	EFX_FILTER_MATCH_ETHER_TYPE =	0x0040,
	EFX_FILTER_MATCH_INNER_VID =	0x0080,
	EFX_FILTER_MATCH_OUTER_VID =	0x0100,
	EFX_FILTER_MATCH_IP_PROTO =	0x0200,
	EFX_FILTER_MATCH_LOC_MAC_IG =	0x0400,
	EFX_FILTER_MATCH_ENCAP_TYPE =	0x0800,
};


enum efx_filter_priority {
	EFX_FILTER_PRI_HINT = 0,
	EFX_FILTER_PRI_AUTO,
	EFX_FILTER_PRI_MANUAL,
	EFX_FILTER_PRI_REQUIRED,
};


enum efx_filter_flags {
	EFX_FILTER_FLAG_RX_RSS = 0x01,
	EFX_FILTER_FLAG_RX_SCATTER = 0x02,
	EFX_FILTER_FLAG_RX_OVER_AUTO = 0x04,
	EFX_FILTER_FLAG_RX = 0x08,
	EFX_FILTER_FLAG_TX = 0x10,
};


enum efx_encap_type {
	EFX_ENCAP_TYPE_NONE = 0,
	EFX_ENCAP_TYPE_VXLAN = 1,
	EFX_ENCAP_TYPE_NVGRE = 2,
	EFX_ENCAP_TYPE_GENEVE = 3,

	EFX_ENCAP_TYPES_MASK = 7,
	EFX_ENCAP_FLAG_IPV6 = 8,
};


struct efx_filter_spec {
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
	u32     encap_type:4;
	
};

enum {
	EFX_FILTER_RX_DMAQ_ID_DROP = 0xfff
};

static inline void efx_filter_init_rx(struct efx_filter_spec *spec,
				      enum efx_filter_priority priority,
				      enum efx_filter_flags flags,
				      unsigned rxq_id)
{
	memset(spec, 0, sizeof(*spec));
	spec->priority = priority;
	spec->flags = EFX_FILTER_FLAG_RX | flags;
	spec->rss_context = 0;
	spec->dmaq_id = rxq_id;
}

static inline void efx_filter_init_tx(struct efx_filter_spec *spec,
				      unsigned txq_id)
{
	memset(spec, 0, sizeof(*spec));
	spec->priority = EFX_FILTER_PRI_REQUIRED;
	spec->flags = EFX_FILTER_FLAG_TX;
	spec->dmaq_id = txq_id;
}


static inline int
efx_filter_set_ipv4_local(struct efx_filter_spec *spec, u8 proto,
			  __be32 host, __be16 port)
{
	spec->match_flags |=
		EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
		EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT;
	spec->ether_type = htons(ETH_P_IP);
	spec->ip_proto = proto;
	spec->loc_host[0] = host;
	spec->loc_port = port;
	return 0;
}


static inline int
efx_filter_set_ipv4_full(struct efx_filter_spec *spec, u8 proto,
			 __be32 lhost, __be16 lport,
			 __be32 rhost, __be16 rport)
{
	spec->match_flags |=
		EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
		EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT |
		EFX_FILTER_MATCH_REM_HOST | EFX_FILTER_MATCH_REM_PORT;
	spec->ether_type = htons(ETH_P_IP);
	spec->ip_proto = proto;
	spec->loc_host[0] = lhost;
	spec->loc_port = lport;
	spec->rem_host[0] = rhost;
	spec->rem_port = rport;
	return 0;
}

enum {
	EFX_FILTER_VID_UNSPEC = 0xffff,
};


static inline int efx_filter_set_eth_local(struct efx_filter_spec *spec,
					   u16 vid, const u8 *addr)
{
	if (vid == EFX_FILTER_VID_UNSPEC && addr == NULL)
		return -EINVAL;

	if (vid != EFX_FILTER_VID_UNSPEC) {
		spec->match_flags |= EFX_FILTER_MATCH_OUTER_VID;
		spec->outer_vid = htons(vid);
	}
	if (addr != NULL) {
		spec->match_flags |= EFX_FILTER_MATCH_LOC_MAC;
		ether_addr_copy(spec->loc_mac, addr);
	}
	return 0;
}


static inline int efx_filter_set_uc_def(struct efx_filter_spec *spec)
{
	spec->match_flags |= EFX_FILTER_MATCH_LOC_MAC_IG;
	return 0;
}


static inline int efx_filter_set_mc_def(struct efx_filter_spec *spec)
{
	spec->match_flags |= EFX_FILTER_MATCH_LOC_MAC_IG;
	spec->loc_mac[0] = 1;
	return 0;
}

static inline void efx_filter_set_encap_type(struct efx_filter_spec *spec,
					     enum efx_encap_type encap_type)
{
	spec->match_flags |= EFX_FILTER_MATCH_ENCAP_TYPE;
	spec->encap_type = encap_type;
}

static inline enum efx_encap_type efx_filter_get_encap_type(
		const struct efx_filter_spec *spec)
{
	if (spec->match_flags & EFX_FILTER_MATCH_ENCAP_TYPE)
		return spec->encap_type;
	return EFX_ENCAP_TYPE_NONE;
}
#endif 
