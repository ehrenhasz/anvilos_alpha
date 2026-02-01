
 

#include <linux/filter.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/net.h>
#include <linux/module.h>
#include <net/ip.h>
#include <net/lwtunnel.h>
#include <net/netevent.h>
#include <net/netns/generic.h>
#include <net/ip6_fib.h>
#include <net/route.h>
#include <net/seg6.h>
#include <linux/seg6.h>
#include <linux/seg6_local.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>
#include <net/dst_cache.h>
#include <net/ip_tunnels.h>
#ifdef CONFIG_IPV6_SEG6_HMAC
#include <net/seg6_hmac.h>
#endif
#include <net/seg6_local.h>
#include <linux/etherdevice.h>
#include <linux/bpf.h>
#include <linux/netfilter.h>

#define SEG6_F_ATTR(i)		BIT(i)

struct seg6_local_lwt;

 
struct seg6_local_lwtunnel_ops {
	int (*build_state)(struct seg6_local_lwt *slwt, const void *cfg,
			   struct netlink_ext_ack *extack);
	void (*destroy_state)(struct seg6_local_lwt *slwt);
};

struct seg6_action_desc {
	int action;
	unsigned long attrs;

	 
	unsigned long optattrs;

	int (*input)(struct sk_buff *skb, struct seg6_local_lwt *slwt);
	int static_headroom;

	struct seg6_local_lwtunnel_ops slwt_ops;
};

struct bpf_lwt_prog {
	struct bpf_prog *prog;
	char *name;
};

 
#define SEG6_LOCAL_LCBLOCK_DBITS	32
#define SEG6_LOCAL_LCNODE_FN_DBITS	16

 
#define next_csid_chk_cntr_bits(blen, flen)		\
	((blen) + (flen) > 128)

#define next_csid_chk_lcblock_bits(blen)		\
({							\
	typeof(blen) __tmp = blen;			\
	(!__tmp || __tmp > 120 || (__tmp & 0x07));	\
})

#define next_csid_chk_lcnode_fn_bits(flen)		\
	next_csid_chk_lcblock_bits(flen)

 
#define SEG6_F_LOCAL_FLAVORS		SEG6_F_ATTR(SEG6_LOCAL_FLAVORS)

#define SEG6_F_LOCAL_FLV_OP(flvname)	BIT(SEG6_LOCAL_FLV_OP_##flvname)
#define SEG6_F_LOCAL_FLV_NEXT_CSID	SEG6_F_LOCAL_FLV_OP(NEXT_CSID)
#define SEG6_F_LOCAL_FLV_PSP		SEG6_F_LOCAL_FLV_OP(PSP)

 
#define SEG6_LOCAL_FLV8986_SUPP_OPS	SEG6_F_LOCAL_FLV_PSP

#define SEG6_LOCAL_END_FLV_SUPP_OPS	(SEG6_F_LOCAL_FLV_NEXT_CSID | \
					 SEG6_LOCAL_FLV8986_SUPP_OPS)
#define SEG6_LOCAL_END_X_FLV_SUPP_OPS	SEG6_F_LOCAL_FLV_NEXT_CSID

struct seg6_flavors_info {
	 
	__u32 flv_ops;

	 
	__u8 lcblock_bits;
	 
	__u8 lcnode_func_bits;
};

enum seg6_end_dt_mode {
	DT_INVALID_MODE	= -EINVAL,
	DT_LEGACY_MODE	= 0,
	DT_VRF_MODE	= 1,
};

struct seg6_end_dt_info {
	enum seg6_end_dt_mode mode;

	struct net *net;
	 
	int vrf_ifindex;
	int vrf_table;

	 
	u16 family;
};

struct pcpu_seg6_local_counters {
	u64_stats_t packets;
	u64_stats_t bytes;
	u64_stats_t errors;

	struct u64_stats_sync syncp;
};

 
struct seg6_local_counters {
	__u64 packets;
	__u64 bytes;
	__u64 errors;
};

#define seg6_local_alloc_pcpu_counters(__gfp)				\
	__netdev_alloc_pcpu_stats(struct pcpu_seg6_local_counters,	\
				  ((__gfp) | __GFP_ZERO))

#define SEG6_F_LOCAL_COUNTERS	SEG6_F_ATTR(SEG6_LOCAL_COUNTERS)

struct seg6_local_lwt {
	int action;
	struct ipv6_sr_hdr *srh;
	int table;
	struct in_addr nh4;
	struct in6_addr nh6;
	int iif;
	int oif;
	struct bpf_lwt_prog bpf;
#ifdef CONFIG_NET_L3_MASTER_DEV
	struct seg6_end_dt_info dt_info;
#endif
	struct seg6_flavors_info flv_info;

	struct pcpu_seg6_local_counters __percpu *pcpu_counters;

	int headroom;
	struct seg6_action_desc *desc;
	 
	unsigned long parsed_optattrs;
};

static struct seg6_local_lwt *seg6_local_lwtunnel(struct lwtunnel_state *lwt)
{
	return (struct seg6_local_lwt *)lwt->data;
}

static struct ipv6_sr_hdr *get_and_validate_srh(struct sk_buff *skb)
{
	struct ipv6_sr_hdr *srh;

	srh = seg6_get_srh(skb, IP6_FH_F_SKIP_RH);
	if (!srh)
		return NULL;

#ifdef CONFIG_IPV6_SEG6_HMAC
	if (!seg6_hmac_validate_skb(skb))
		return NULL;
#endif

	return srh;
}

static bool decap_and_validate(struct sk_buff *skb, int proto)
{
	struct ipv6_sr_hdr *srh;
	unsigned int off = 0;

	srh = seg6_get_srh(skb, 0);
	if (srh && srh->segments_left > 0)
		return false;

#ifdef CONFIG_IPV6_SEG6_HMAC
	if (srh && !seg6_hmac_validate_skb(skb))
		return false;
#endif

	if (ipv6_find_hdr(skb, &off, proto, NULL, NULL) < 0)
		return false;

	if (!pskb_pull(skb, off))
		return false;

	skb_postpull_rcsum(skb, skb_network_header(skb), off);

	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	if (iptunnel_pull_offloads(skb))
		return false;

	return true;
}

static void advance_nextseg(struct ipv6_sr_hdr *srh, struct in6_addr *daddr)
{
	struct in6_addr *addr;

	srh->segments_left--;
	addr = srh->segments + srh->segments_left;
	*daddr = *addr;
}

static int
seg6_lookup_any_nexthop(struct sk_buff *skb, struct in6_addr *nhaddr,
			u32 tbl_id, bool local_delivery)
{
	struct net *net = dev_net(skb->dev);
	struct ipv6hdr *hdr = ipv6_hdr(skb);
	int flags = RT6_LOOKUP_F_HAS_SADDR;
	struct dst_entry *dst = NULL;
	struct rt6_info *rt;
	struct flowi6 fl6;
	int dev_flags = 0;

	memset(&fl6, 0, sizeof(fl6));
	fl6.flowi6_iif = skb->dev->ifindex;
	fl6.daddr = nhaddr ? *nhaddr : hdr->daddr;
	fl6.saddr = hdr->saddr;
	fl6.flowlabel = ip6_flowinfo(hdr);
	fl6.flowi6_mark = skb->mark;
	fl6.flowi6_proto = hdr->nexthdr;

	if (nhaddr)
		fl6.flowi6_flags = FLOWI_FLAG_KNOWN_NH;

	if (!tbl_id) {
		dst = ip6_route_input_lookup(net, skb->dev, &fl6, skb, flags);
	} else {
		struct fib6_table *table;

		table = fib6_get_table(net, tbl_id);
		if (!table)
			goto out;

		rt = ip6_pol_route(net, table, 0, &fl6, skb, flags);
		dst = &rt->dst;
	}

	 
	if (!local_delivery)
		dev_flags |= IFF_LOOPBACK;

	if (dst && (dst->dev->flags & dev_flags) && !dst->error) {
		dst_release(dst);
		dst = NULL;
	}

out:
	if (!dst) {
		rt = net->ipv6.ip6_blk_hole_entry;
		dst = &rt->dst;
		dst_hold(dst);
	}

	skb_dst_drop(skb);
	skb_dst_set(skb, dst);
	return dst->error;
}

int seg6_lookup_nexthop(struct sk_buff *skb,
			struct in6_addr *nhaddr, u32 tbl_id)
{
	return seg6_lookup_any_nexthop(skb, nhaddr, tbl_id, false);
}

static __u8 seg6_flv_lcblock_octects(const struct seg6_flavors_info *finfo)
{
	return finfo->lcblock_bits >> 3;
}

static __u8 seg6_flv_lcnode_func_octects(const struct seg6_flavors_info *finfo)
{
	return finfo->lcnode_func_bits >> 3;
}

static bool seg6_next_csid_is_arg_zero(const struct in6_addr *addr,
				       const struct seg6_flavors_info *finfo)
{
	__u8 fnc_octects = seg6_flv_lcnode_func_octects(finfo);
	__u8 blk_octects = seg6_flv_lcblock_octects(finfo);
	__u8 arg_octects;
	int i;

	arg_octects = 16 - blk_octects - fnc_octects;
	for (i = 0; i < arg_octects; ++i) {
		if (addr->s6_addr[blk_octects + fnc_octects + i] != 0x00)
			return false;
	}

	return true;
}

 
static void seg6_next_csid_advance_arg(struct in6_addr *addr,
				       const struct seg6_flavors_info *finfo)
{
	__u8 fnc_octects = seg6_flv_lcnode_func_octects(finfo);
	__u8 blk_octects = seg6_flv_lcblock_octects(finfo);

	 
	memmove(&addr->s6_addr[blk_octects],
		&addr->s6_addr[blk_octects + fnc_octects],
		16 - blk_octects - fnc_octects);

	memset(&addr->s6_addr[16 - fnc_octects], 0x00, fnc_octects);
}

static int input_action_end_finish(struct sk_buff *skb,
				   struct seg6_local_lwt *slwt)
{
	seg6_lookup_nexthop(skb, NULL, 0);

	return dst_input(skb);
}

static int input_action_end_core(struct sk_buff *skb,
				 struct seg6_local_lwt *slwt)
{
	struct ipv6_sr_hdr *srh;

	srh = get_and_validate_srh(skb);
	if (!srh)
		goto drop;

	advance_nextseg(srh, &ipv6_hdr(skb)->daddr);

	return input_action_end_finish(skb, slwt);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

static int end_next_csid_core(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	const struct seg6_flavors_info *finfo = &slwt->flv_info;
	struct in6_addr *daddr = &ipv6_hdr(skb)->daddr;

	if (seg6_next_csid_is_arg_zero(daddr, finfo))
		return input_action_end_core(skb, slwt);

	 
	seg6_next_csid_advance_arg(daddr, finfo);

	return input_action_end_finish(skb, slwt);
}

static int input_action_end_x_finish(struct sk_buff *skb,
				     struct seg6_local_lwt *slwt)
{
	seg6_lookup_nexthop(skb, &slwt->nh6, 0);

	return dst_input(skb);
}

static int input_action_end_x_core(struct sk_buff *skb,
				   struct seg6_local_lwt *slwt)
{
	struct ipv6_sr_hdr *srh;

	srh = get_and_validate_srh(skb);
	if (!srh)
		goto drop;

	advance_nextseg(srh, &ipv6_hdr(skb)->daddr);

	return input_action_end_x_finish(skb, slwt);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

static int end_x_next_csid_core(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	const struct seg6_flavors_info *finfo = &slwt->flv_info;
	struct in6_addr *daddr = &ipv6_hdr(skb)->daddr;

	if (seg6_next_csid_is_arg_zero(daddr, finfo))
		return input_action_end_x_core(skb, slwt);

	 
	seg6_next_csid_advance_arg(daddr, finfo);

	return input_action_end_x_finish(skb, slwt);
}

static bool seg6_next_csid_enabled(__u32 fops)
{
	return fops & SEG6_F_LOCAL_FLV_NEXT_CSID;
}

 
static int seg6_flv_supp_ops_by_action(int action, __u32 *fops)
{
	switch (action) {
	case SEG6_LOCAL_ACTION_END:
		*fops = SEG6_LOCAL_END_FLV_SUPP_OPS;
		break;
	case SEG6_LOCAL_ACTION_END_X:
		*fops = SEG6_LOCAL_END_X_FLV_SUPP_OPS;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

 
enum seg6_local_pktinfo {
	 
	SEG6_LOCAL_PKTINFO_NOHDR	= 0,
	SEG6_LOCAL_PKTINFO_SL_ZERO,
	SEG6_LOCAL_PKTINFO_SL_ONE,
	SEG6_LOCAL_PKTINFO_SL_MORE,
	__SEG6_LOCAL_PKTINFO_MAX,
};

#define SEG6_LOCAL_PKTINFO_MAX (__SEG6_LOCAL_PKTINFO_MAX - 1)

static enum seg6_local_pktinfo seg6_get_srh_pktinfo(struct ipv6_sr_hdr *srh)
{
	__u8 sgl;

	if (!srh)
		return SEG6_LOCAL_PKTINFO_NOHDR;

	sgl = srh->segments_left;
	if (sgl < 2)
		return SEG6_LOCAL_PKTINFO_SL_ZERO + sgl;

	return SEG6_LOCAL_PKTINFO_SL_MORE;
}

enum seg6_local_flv_action {
	SEG6_LOCAL_FLV_ACT_UNSPEC	= 0,
	SEG6_LOCAL_FLV_ACT_END,
	SEG6_LOCAL_FLV_ACT_PSP,
	SEG6_LOCAL_FLV_ACT_USP,
	SEG6_LOCAL_FLV_ACT_USD,
	__SEG6_LOCAL_FLV_ACT_MAX
};

#define SEG6_LOCAL_FLV_ACT_MAX (__SEG6_LOCAL_FLV_ACT_MAX - 1)

 
#define flv8986_act_tbl_idx(pf, fm)					\
	((((pf) << bits_per(SEG6_LOCAL_FLV8986_SUPP_OPS)) |		\
	  ((fm) & SEG6_LOCAL_FLV8986_SUPP_OPS)) >> SEG6_LOCAL_FLV_OP_PSP)

 
#define FLV8986_ACT_TBL_SIZE						\
	roundup_pow_of_two(flv8986_act_tbl_idx(SEG6_LOCAL_PKTINFO_MAX,	\
					       SEG6_LOCAL_FLV8986_SUPP_OPS))

 
#define tbl_cfg(act, pf, fm)						\
	[flv8986_act_tbl_idx(SEG6_LOCAL_PKTINFO_##pf,			\
			     (fm))] = SEG6_LOCAL_FLV_ACT_##act

 
#define F_PSP	SEG6_F_LOCAL_FLV_PSP

 
static const u8 flv8986_act_tbl[FLV8986_ACT_TBL_SIZE] = {
	 
	tbl_cfg(PSP, SL_ONE, F_PSP),
	 
	tbl_cfg(END, SL_MORE, F_PSP),
};

#undef F_PSP
#undef tbl_cfg

 
static enum seg6_local_flv_action
seg6_local_flv8986_act_lookup(enum seg6_local_pktinfo pinfo, __u32 flvmask)
{
	unsigned long index;

	 
	if (unlikely(flvmask & ~SEG6_LOCAL_FLV8986_SUPP_OPS))
		return SEG6_LOCAL_FLV_ACT_UNSPEC;

	index = flv8986_act_tbl_idx(pinfo, flvmask);
	if (unlikely(index >= FLV8986_ACT_TBL_SIZE))
		return SEG6_LOCAL_FLV_ACT_UNSPEC;

	return flv8986_act_tbl[index];
}

 
static bool seg6_pop_srh(struct sk_buff *skb, int srhoff)
{
	struct ipv6_sr_hdr *srh;
	struct ipv6hdr *iph;
	__u8 srh_nexthdr;
	int thoff = -1;
	int srhlen;
	int nhlen;

	if (unlikely(srhoff < sizeof(*iph) ||
		     !pskb_may_pull(skb, srhoff + sizeof(*srh))))
		return false;

	srh = (struct ipv6_sr_hdr *)(skb->data + srhoff);
	srhlen = ipv6_optlen(srh);

	 
	if (unlikely(skb_ensure_writable(skb, srhoff + srhlen)))
		return false;

	 
	srh = (struct ipv6_sr_hdr *)(skb->data + srhoff);
	srh_nexthdr = srh->nexthdr;

	if (unlikely(!skb_transport_header_was_set(skb)))
		goto pull;

	nhlen = skb_network_header_len(skb);
	 
	if (likely(nhlen <= srhoff))
		thoff = nhlen;
	else if (nhlen >= srhoff + srhlen)
		 
		thoff = nhlen - srhlen;
	else
		 
		return false;
pull:
	 
	skb_pull_rcsum(skb, srhoff + srhlen);
	memmove(skb_network_header(skb) + srhlen, skb_network_header(skb),
		srhoff);
	skb_push(skb, srhoff);

	skb_reset_network_header(skb);
	skb_mac_header_rebuild(skb);
	if (likely(thoff >= 0))
		skb_set_transport_header(skb, thoff);

	iph = ipv6_hdr(skb);
	if (iph->nexthdr == NEXTHDR_ROUTING) {
		iph->nexthdr = srh_nexthdr;
	} else {
		 
		unsigned int off = sizeof(*iph);
		struct ipv6_opt_hdr *hp, _hdr;
		__u8 nexthdr = iph->nexthdr;

		for (;;) {
			if (unlikely(!ipv6_ext_hdr(nexthdr) ||
				     nexthdr == NEXTHDR_NONE))
				return false;

			hp = skb_header_pointer(skb, off, sizeof(_hdr), &_hdr);
			if (unlikely(!hp))
				return false;

			if (hp->nexthdr == NEXTHDR_ROUTING) {
				hp->nexthdr = srh_nexthdr;
				break;
			}

			switch (nexthdr) {
			case NEXTHDR_FRAGMENT:
				fallthrough;
			case NEXTHDR_AUTH:
				 
				return false;
			default:
				off += ipv6_optlen(hp);
				break;
			}

			nexthdr = hp->nexthdr;
		}
	}

	iph->payload_len = htons(skb->len - sizeof(struct ipv6hdr));

	skb_postpush_rcsum(skb, iph, srhoff);

	return true;
}

 
static int end_flv8986_core(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	const struct seg6_flavors_info *finfo = &slwt->flv_info;
	enum seg6_local_flv_action action;
	enum seg6_local_pktinfo pinfo;
	struct ipv6_sr_hdr *srh;
	__u32 flvmask;
	int srhoff;

	srh = seg6_get_srh(skb, 0);
	srhoff = srh ? ((unsigned char *)srh - skb->data) : 0;
	pinfo = seg6_get_srh_pktinfo(srh);
#ifdef CONFIG_IPV6_SEG6_HMAC
	if (srh && !seg6_hmac_validate_skb(skb))
		goto drop;
#endif
	flvmask = finfo->flv_ops;
	if (unlikely(flvmask & ~SEG6_LOCAL_FLV8986_SUPP_OPS)) {
		pr_warn_once("seg6local: invalid RFC8986 flavors\n");
		goto drop;
	}

	 
	action = seg6_local_flv8986_act_lookup(pinfo, flvmask);
	switch (action) {
	case SEG6_LOCAL_FLV_ACT_END:
		 
		advance_nextseg(srh, &ipv6_hdr(skb)->daddr);
		break;
	case SEG6_LOCAL_FLV_ACT_PSP:
		advance_nextseg(srh, &ipv6_hdr(skb)->daddr);

		if (unlikely(!seg6_pop_srh(skb, srhoff)))
			goto drop;
		break;
	case SEG6_LOCAL_FLV_ACT_UNSPEC:
		fallthrough;
	default:
		 
		goto drop;
	}

	return input_action_end_finish(skb, slwt);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

 
static int input_action_end(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	const struct seg6_flavors_info *finfo = &slwt->flv_info;
	__u32 fops = finfo->flv_ops;

	if (!fops)
		return input_action_end_core(skb, slwt);

	 
	if (seg6_next_csid_enabled(fops))
		return end_next_csid_core(skb, slwt);

	 
	return end_flv8986_core(skb, slwt);
}

 
static int input_action_end_x(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	const struct seg6_flavors_info *finfo = &slwt->flv_info;
	__u32 fops = finfo->flv_ops;

	 
	if (seg6_next_csid_enabled(fops))
		return end_x_next_csid_core(skb, slwt);

	return input_action_end_x_core(skb, slwt);
}

static int input_action_end_t(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct ipv6_sr_hdr *srh;

	srh = get_and_validate_srh(skb);
	if (!srh)
		goto drop;

	advance_nextseg(srh, &ipv6_hdr(skb)->daddr);

	seg6_lookup_nexthop(skb, NULL, slwt->table);

	return dst_input(skb);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

 
static int input_action_end_dx2(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	struct net *net = dev_net(skb->dev);
	struct net_device *odev;
	struct ethhdr *eth;

	if (!decap_and_validate(skb, IPPROTO_ETHERNET))
		goto drop;

	if (!pskb_may_pull(skb, ETH_HLEN))
		goto drop;

	skb_reset_mac_header(skb);
	eth = (struct ethhdr *)skb->data;

	 
	if (!eth_proto_is_802_3(eth->h_proto))
		goto drop;

	odev = dev_get_by_index_rcu(net, slwt->oif);
	if (!odev)
		goto drop;

	 
	if (odev->type != ARPHRD_ETHER)
		goto drop;

	if (!(odev->flags & IFF_UP) || !netif_carrier_ok(odev))
		goto drop;

	skb_orphan(skb);

	if (skb_warn_if_lro(skb))
		goto drop;

	skb_forward_csum(skb);

	if (skb->len - ETH_HLEN > odev->mtu)
		goto drop;

	skb->dev = odev;
	skb->protocol = eth->h_proto;

	return dev_queue_xmit(skb);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

static int input_action_end_dx6_finish(struct net *net, struct sock *sk,
				       struct sk_buff *skb)
{
	struct dst_entry *orig_dst = skb_dst(skb);
	struct in6_addr *nhaddr = NULL;
	struct seg6_local_lwt *slwt;

	slwt = seg6_local_lwtunnel(orig_dst->lwtstate);

	 
	if (!ipv6_addr_any(&slwt->nh6))
		nhaddr = &slwt->nh6;

	seg6_lookup_nexthop(skb, nhaddr, 0);

	return dst_input(skb);
}

 
static int input_action_end_dx6(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	 

	if (!decap_and_validate(skb, IPPROTO_IPV6))
		goto drop;

	if (!pskb_may_pull(skb, sizeof(struct ipv6hdr)))
		goto drop;

	skb_set_transport_header(skb, sizeof(struct ipv6hdr));
	nf_reset_ct(skb);

	if (static_branch_unlikely(&nf_hooks_lwtunnel_enabled))
		return NF_HOOK(NFPROTO_IPV6, NF_INET_PRE_ROUTING,
			       dev_net(skb->dev), NULL, skb, NULL,
			       skb_dst(skb)->dev, input_action_end_dx6_finish);

	return input_action_end_dx6_finish(dev_net(skb->dev), NULL, skb);
drop:
	kfree_skb(skb);
	return -EINVAL;
}

static int input_action_end_dx4_finish(struct net *net, struct sock *sk,
				       struct sk_buff *skb)
{
	struct dst_entry *orig_dst = skb_dst(skb);
	struct seg6_local_lwt *slwt;
	struct iphdr *iph;
	__be32 nhaddr;
	int err;

	slwt = seg6_local_lwtunnel(orig_dst->lwtstate);

	iph = ip_hdr(skb);

	nhaddr = slwt->nh4.s_addr ?: iph->daddr;

	skb_dst_drop(skb);

	err = ip_route_input(skb, nhaddr, iph->saddr, 0, skb->dev);
	if (err) {
		kfree_skb(skb);
		return -EINVAL;
	}

	return dst_input(skb);
}

static int input_action_end_dx4(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	if (!decap_and_validate(skb, IPPROTO_IPIP))
		goto drop;

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto drop;

	skb->protocol = htons(ETH_P_IP);
	skb_set_transport_header(skb, sizeof(struct iphdr));
	nf_reset_ct(skb);

	if (static_branch_unlikely(&nf_hooks_lwtunnel_enabled))
		return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING,
			       dev_net(skb->dev), NULL, skb, NULL,
			       skb_dst(skb)->dev, input_action_end_dx4_finish);

	return input_action_end_dx4_finish(dev_net(skb->dev), NULL, skb);
drop:
	kfree_skb(skb);
	return -EINVAL;
}

#ifdef CONFIG_NET_L3_MASTER_DEV
static struct net *fib6_config_get_net(const struct fib6_config *fib6_cfg)
{
	const struct nl_info *nli = &fib6_cfg->fc_nlinfo;

	return nli->nl_net;
}

static int __seg6_end_dt_vrf_build(struct seg6_local_lwt *slwt, const void *cfg,
				   u16 family, struct netlink_ext_ack *extack)
{
	struct seg6_end_dt_info *info = &slwt->dt_info;
	int vrf_ifindex;
	struct net *net;

	net = fib6_config_get_net(cfg);

	 
	vrf_ifindex = l3mdev_ifindex_lookup_by_table_id(L3MDEV_TYPE_VRF, net,
							info->vrf_table);
	if (vrf_ifindex < 0) {
		if (vrf_ifindex == -EPERM) {
			NL_SET_ERR_MSG(extack,
				       "Strict mode for VRF is disabled");
		} else if (vrf_ifindex == -ENODEV) {
			NL_SET_ERR_MSG(extack,
				       "Table has no associated VRF device");
		} else {
			pr_debug("seg6local: SRv6 End.DT* creation error=%d\n",
				 vrf_ifindex);
		}

		return vrf_ifindex;
	}

	info->net = net;
	info->vrf_ifindex = vrf_ifindex;

	info->family = family;
	info->mode = DT_VRF_MODE;

	return 0;
}

 
static struct sk_buff *end_dt_vrf_rcv(struct sk_buff *skb, u16 family,
				      struct net_device *dev)
{
	 
	if (unlikely(!netif_is_l3_master(dev) && !netif_has_l3_rx_handler(dev)))
		goto drop;

	if (unlikely(!dev->l3mdev_ops->l3mdev_l3_rcv))
		goto drop;

	 
	skb_unset_mac_header(skb);

	skb = dev->l3mdev_ops->l3mdev_l3_rcv(dev, skb, family);
	if (!skb)
		 
		return NULL;

	 
	if (unlikely(skb->dev != dev || skb->skb_iif != dev->ifindex))
		goto drop;

	return skb;

drop:
	kfree_skb(skb);
	return ERR_PTR(-EINVAL);
}

static struct net_device *end_dt_get_vrf_rcu(struct sk_buff *skb,
					     struct seg6_end_dt_info *info)
{
	int vrf_ifindex = info->vrf_ifindex;
	struct net *net = info->net;

	if (unlikely(vrf_ifindex < 0))
		goto error;

	if (unlikely(!net_eq(dev_net(skb->dev), net)))
		goto error;

	return dev_get_by_index_rcu(net, vrf_ifindex);

error:
	return NULL;
}

static struct sk_buff *end_dt_vrf_core(struct sk_buff *skb,
				       struct seg6_local_lwt *slwt, u16 family)
{
	struct seg6_end_dt_info *info = &slwt->dt_info;
	struct net_device *vrf;
	__be16 protocol;
	int hdrlen;

	vrf = end_dt_get_vrf_rcu(skb, info);
	if (unlikely(!vrf))
		goto drop;

	switch (family) {
	case AF_INET:
		protocol = htons(ETH_P_IP);
		hdrlen = sizeof(struct iphdr);
		break;
	case AF_INET6:
		protocol = htons(ETH_P_IPV6);
		hdrlen = sizeof(struct ipv6hdr);
		break;
	case AF_UNSPEC:
		fallthrough;
	default:
		goto drop;
	}

	if (unlikely(info->family != AF_UNSPEC && info->family != family)) {
		pr_warn_once("seg6local: SRv6 End.DT* family mismatch");
		goto drop;
	}

	skb->protocol = protocol;

	skb_dst_drop(skb);

	skb_set_transport_header(skb, hdrlen);
	nf_reset_ct(skb);

	return end_dt_vrf_rcv(skb, family, vrf);

drop:
	kfree_skb(skb);
	return ERR_PTR(-EINVAL);
}

static int input_action_end_dt4(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	struct iphdr *iph;
	int err;

	if (!decap_and_validate(skb, IPPROTO_IPIP))
		goto drop;

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto drop;

	skb = end_dt_vrf_core(skb, slwt, AF_INET);
	if (!skb)
		 
		return 0;

	if (IS_ERR(skb))
		return PTR_ERR(skb);

	iph = ip_hdr(skb);

	err = ip_route_input(skb, iph->daddr, iph->saddr, 0, skb->dev);
	if (unlikely(err))
		goto drop;

	return dst_input(skb);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

static int seg6_end_dt4_build(struct seg6_local_lwt *slwt, const void *cfg,
			      struct netlink_ext_ack *extack)
{
	return __seg6_end_dt_vrf_build(slwt, cfg, AF_INET, extack);
}

static enum
seg6_end_dt_mode seg6_end_dt6_parse_mode(struct seg6_local_lwt *slwt)
{
	unsigned long parsed_optattrs = slwt->parsed_optattrs;
	bool legacy, vrfmode;

	legacy	= !!(parsed_optattrs & SEG6_F_ATTR(SEG6_LOCAL_TABLE));
	vrfmode	= !!(parsed_optattrs & SEG6_F_ATTR(SEG6_LOCAL_VRFTABLE));

	if (!(legacy ^ vrfmode))
		 
		return DT_INVALID_MODE;

	return legacy ? DT_LEGACY_MODE : DT_VRF_MODE;
}

static enum seg6_end_dt_mode seg6_end_dt6_get_mode(struct seg6_local_lwt *slwt)
{
	struct seg6_end_dt_info *info = &slwt->dt_info;

	return info->mode;
}

static int seg6_end_dt6_build(struct seg6_local_lwt *slwt, const void *cfg,
			      struct netlink_ext_ack *extack)
{
	enum seg6_end_dt_mode mode = seg6_end_dt6_parse_mode(slwt);
	struct seg6_end_dt_info *info = &slwt->dt_info;

	switch (mode) {
	case DT_LEGACY_MODE:
		info->mode = DT_LEGACY_MODE;
		return 0;
	case DT_VRF_MODE:
		return __seg6_end_dt_vrf_build(slwt, cfg, AF_INET6, extack);
	default:
		NL_SET_ERR_MSG(extack, "table or vrftable must be specified");
		return -EINVAL;
	}
}
#endif

static int input_action_end_dt6(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	if (!decap_and_validate(skb, IPPROTO_IPV6))
		goto drop;

	if (!pskb_may_pull(skb, sizeof(struct ipv6hdr)))
		goto drop;

#ifdef CONFIG_NET_L3_MASTER_DEV
	if (seg6_end_dt6_get_mode(slwt) == DT_LEGACY_MODE)
		goto legacy_mode;

	 
	skb = end_dt_vrf_core(skb, slwt, AF_INET6);
	if (!skb)
		 
		return 0;

	if (IS_ERR(skb))
		return PTR_ERR(skb);

	 
	seg6_lookup_any_nexthop(skb, NULL, 0, true);

	return dst_input(skb);

legacy_mode:
#endif
	skb_set_transport_header(skb, sizeof(struct ipv6hdr));

	seg6_lookup_any_nexthop(skb, NULL, slwt->table, true);

	return dst_input(skb);

drop:
	kfree_skb(skb);
	return -EINVAL;
}

#ifdef CONFIG_NET_L3_MASTER_DEV
static int seg6_end_dt46_build(struct seg6_local_lwt *slwt, const void *cfg,
			       struct netlink_ext_ack *extack)
{
	return __seg6_end_dt_vrf_build(slwt, cfg, AF_UNSPEC, extack);
}

static int input_action_end_dt46(struct sk_buff *skb,
				 struct seg6_local_lwt *slwt)
{
	unsigned int off = 0;
	int nexthdr;

	nexthdr = ipv6_find_hdr(skb, &off, -1, NULL, NULL);
	if (unlikely(nexthdr < 0))
		goto drop;

	switch (nexthdr) {
	case IPPROTO_IPIP:
		return input_action_end_dt4(skb, slwt);
	case IPPROTO_IPV6:
		return input_action_end_dt6(skb, slwt);
	}

drop:
	kfree_skb(skb);
	return -EINVAL;
}
#endif

 
static int input_action_end_b6(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct ipv6_sr_hdr *srh;
	int err = -EINVAL;

	srh = get_and_validate_srh(skb);
	if (!srh)
		goto drop;

	err = seg6_do_srh_inline(skb, slwt->srh);
	if (err)
		goto drop;

	skb_set_transport_header(skb, sizeof(struct ipv6hdr));

	seg6_lookup_nexthop(skb, NULL, 0);

	return dst_input(skb);

drop:
	kfree_skb(skb);
	return err;
}

 
static int input_action_end_b6_encap(struct sk_buff *skb,
				     struct seg6_local_lwt *slwt)
{
	struct ipv6_sr_hdr *srh;
	int err = -EINVAL;

	srh = get_and_validate_srh(skb);
	if (!srh)
		goto drop;

	advance_nextseg(srh, &ipv6_hdr(skb)->daddr);

	skb_reset_inner_headers(skb);
	skb->encapsulation = 1;

	err = seg6_do_srh_encap(skb, slwt->srh, IPPROTO_IPV6);
	if (err)
		goto drop;

	skb_set_transport_header(skb, sizeof(struct ipv6hdr));

	seg6_lookup_nexthop(skb, NULL, 0);

	return dst_input(skb);

drop:
	kfree_skb(skb);
	return err;
}

DEFINE_PER_CPU(struct seg6_bpf_srh_state, seg6_bpf_srh_states);

bool seg6_bpf_has_valid_srh(struct sk_buff *skb)
{
	struct seg6_bpf_srh_state *srh_state =
		this_cpu_ptr(&seg6_bpf_srh_states);
	struct ipv6_sr_hdr *srh = srh_state->srh;

	if (unlikely(srh == NULL))
		return false;

	if (unlikely(!srh_state->valid)) {
		if ((srh_state->hdrlen & 7) != 0)
			return false;

		srh->hdrlen = (u8)(srh_state->hdrlen >> 3);
		if (!seg6_validate_srh(srh, (srh->hdrlen + 1) << 3, true))
			return false;

		srh_state->valid = true;
	}

	return true;
}

static int input_action_end_bpf(struct sk_buff *skb,
				struct seg6_local_lwt *slwt)
{
	struct seg6_bpf_srh_state *srh_state =
		this_cpu_ptr(&seg6_bpf_srh_states);
	struct ipv6_sr_hdr *srh;
	int ret;

	srh = get_and_validate_srh(skb);
	if (!srh) {
		kfree_skb(skb);
		return -EINVAL;
	}
	advance_nextseg(srh, &ipv6_hdr(skb)->daddr);

	 
	preempt_disable();
	srh_state->srh = srh;
	srh_state->hdrlen = srh->hdrlen << 3;
	srh_state->valid = true;

	rcu_read_lock();
	bpf_compute_data_pointers(skb);
	ret = bpf_prog_run_save_cb(slwt->bpf.prog, skb);
	rcu_read_unlock();

	switch (ret) {
	case BPF_OK:
	case BPF_REDIRECT:
		break;
	case BPF_DROP:
		goto drop;
	default:
		pr_warn_once("bpf-seg6local: Illegal return value %u\n", ret);
		goto drop;
	}

	if (srh_state->srh && !seg6_bpf_has_valid_srh(skb))
		goto drop;

	preempt_enable();
	if (ret != BPF_REDIRECT)
		seg6_lookup_nexthop(skb, NULL, 0);

	return dst_input(skb);

drop:
	preempt_enable();
	kfree_skb(skb);
	return -EINVAL;
}

static struct seg6_action_desc seg6_action_table[] = {
	{
		.action		= SEG6_LOCAL_ACTION_END,
		.attrs		= 0,
		.optattrs	= SEG6_F_LOCAL_COUNTERS |
				  SEG6_F_LOCAL_FLAVORS,
		.input		= input_action_end,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_X,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_NH6),
		.optattrs	= SEG6_F_LOCAL_COUNTERS |
				  SEG6_F_LOCAL_FLAVORS,
		.input		= input_action_end_x,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_T,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_TABLE),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_t,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_DX2,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_OIF),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_dx2,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_DX6,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_NH6),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_dx6,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_DX4,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_NH4),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_dx4,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_DT4,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_VRFTABLE),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
#ifdef CONFIG_NET_L3_MASTER_DEV
		.input		= input_action_end_dt4,
		.slwt_ops	= {
					.build_state = seg6_end_dt4_build,
				  },
#endif
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_DT6,
#ifdef CONFIG_NET_L3_MASTER_DEV
		.attrs		= 0,
		.optattrs	= SEG6_F_LOCAL_COUNTERS		|
				  SEG6_F_ATTR(SEG6_LOCAL_TABLE) |
				  SEG6_F_ATTR(SEG6_LOCAL_VRFTABLE),
		.slwt_ops	= {
					.build_state = seg6_end_dt6_build,
				  },
#else
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_TABLE),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
#endif
		.input		= input_action_end_dt6,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_DT46,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_VRFTABLE),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
#ifdef CONFIG_NET_L3_MASTER_DEV
		.input		= input_action_end_dt46,
		.slwt_ops	= {
					.build_state = seg6_end_dt46_build,
				  },
#endif
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_B6,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_SRH),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_b6,
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_B6_ENCAP,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_SRH),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_b6_encap,
		.static_headroom	= sizeof(struct ipv6hdr),
	},
	{
		.action		= SEG6_LOCAL_ACTION_END_BPF,
		.attrs		= SEG6_F_ATTR(SEG6_LOCAL_BPF),
		.optattrs	= SEG6_F_LOCAL_COUNTERS,
		.input		= input_action_end_bpf,
	},

};

static struct seg6_action_desc *__get_action_desc(int action)
{
	struct seg6_action_desc *desc;
	int i, count;

	count = ARRAY_SIZE(seg6_action_table);
	for (i = 0; i < count; i++) {
		desc = &seg6_action_table[i];
		if (desc->action == action)
			return desc;
	}

	return NULL;
}

static bool seg6_lwtunnel_counters_enabled(struct seg6_local_lwt *slwt)
{
	return slwt->parsed_optattrs & SEG6_F_LOCAL_COUNTERS;
}

static void seg6_local_update_counters(struct seg6_local_lwt *slwt,
				       unsigned int len, int err)
{
	struct pcpu_seg6_local_counters *pcounters;

	pcounters = this_cpu_ptr(slwt->pcpu_counters);
	u64_stats_update_begin(&pcounters->syncp);

	if (likely(!err)) {
		u64_stats_inc(&pcounters->packets);
		u64_stats_add(&pcounters->bytes, len);
	} else {
		u64_stats_inc(&pcounters->errors);
	}

	u64_stats_update_end(&pcounters->syncp);
}

static int seg6_local_input_core(struct net *net, struct sock *sk,
				 struct sk_buff *skb)
{
	struct dst_entry *orig_dst = skb_dst(skb);
	struct seg6_action_desc *desc;
	struct seg6_local_lwt *slwt;
	unsigned int len = skb->len;
	int rc;

	slwt = seg6_local_lwtunnel(orig_dst->lwtstate);
	desc = slwt->desc;

	rc = desc->input(skb, slwt);

	if (!seg6_lwtunnel_counters_enabled(slwt))
		return rc;

	seg6_local_update_counters(slwt, len, rc);

	return rc;
}

static int seg6_local_input(struct sk_buff *skb)
{
	if (skb->protocol != htons(ETH_P_IPV6)) {
		kfree_skb(skb);
		return -EINVAL;
	}

	if (static_branch_unlikely(&nf_hooks_lwtunnel_enabled))
		return NF_HOOK(NFPROTO_IPV6, NF_INET_LOCAL_IN,
			       dev_net(skb->dev), NULL, skb, skb->dev, NULL,
			       seg6_local_input_core);

	return seg6_local_input_core(dev_net(skb->dev), NULL, skb);
}

static const struct nla_policy seg6_local_policy[SEG6_LOCAL_MAX + 1] = {
	[SEG6_LOCAL_ACTION]	= { .type = NLA_U32 },
	[SEG6_LOCAL_SRH]	= { .type = NLA_BINARY },
	[SEG6_LOCAL_TABLE]	= { .type = NLA_U32 },
	[SEG6_LOCAL_VRFTABLE]	= { .type = NLA_U32 },
	[SEG6_LOCAL_NH4]	= { .type = NLA_BINARY,
				    .len = sizeof(struct in_addr) },
	[SEG6_LOCAL_NH6]	= { .type = NLA_BINARY,
				    .len = sizeof(struct in6_addr) },
	[SEG6_LOCAL_IIF]	= { .type = NLA_U32 },
	[SEG6_LOCAL_OIF]	= { .type = NLA_U32 },
	[SEG6_LOCAL_BPF]	= { .type = NLA_NESTED },
	[SEG6_LOCAL_COUNTERS]	= { .type = NLA_NESTED },
	[SEG6_LOCAL_FLAVORS]	= { .type = NLA_NESTED },
};

static int parse_nla_srh(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			 struct netlink_ext_ack *extack)
{
	struct ipv6_sr_hdr *srh;
	int len;

	srh = nla_data(attrs[SEG6_LOCAL_SRH]);
	len = nla_len(attrs[SEG6_LOCAL_SRH]);

	 
	if (len < sizeof(*srh) + sizeof(struct in6_addr))
		return -EINVAL;

	if (!seg6_validate_srh(srh, len, false))
		return -EINVAL;

	slwt->srh = kmemdup(srh, len, GFP_KERNEL);
	if (!slwt->srh)
		return -ENOMEM;

	slwt->headroom += len;

	return 0;
}

static int put_nla_srh(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct ipv6_sr_hdr *srh;
	struct nlattr *nla;
	int len;

	srh = slwt->srh;
	len = (srh->hdrlen + 1) << 3;

	nla = nla_reserve(skb, SEG6_LOCAL_SRH, len);
	if (!nla)
		return -EMSGSIZE;

	memcpy(nla_data(nla), srh, len);

	return 0;
}

static int cmp_nla_srh(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	int len = (a->srh->hdrlen + 1) << 3;

	if (len != ((b->srh->hdrlen + 1) << 3))
		return 1;

	return memcmp(a->srh, b->srh, len);
}

static void destroy_attr_srh(struct seg6_local_lwt *slwt)
{
	kfree(slwt->srh);
}

static int parse_nla_table(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			   struct netlink_ext_ack *extack)
{
	slwt->table = nla_get_u32(attrs[SEG6_LOCAL_TABLE]);

	return 0;
}

static int put_nla_table(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	if (nla_put_u32(skb, SEG6_LOCAL_TABLE, slwt->table))
		return -EMSGSIZE;

	return 0;
}

static int cmp_nla_table(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	if (a->table != b->table)
		return 1;

	return 0;
}

static struct
seg6_end_dt_info *seg6_possible_end_dt_info(struct seg6_local_lwt *slwt)
{
#ifdef CONFIG_NET_L3_MASTER_DEV
	return &slwt->dt_info;
#else
	return ERR_PTR(-EOPNOTSUPP);
#endif
}

static int parse_nla_vrftable(struct nlattr **attrs,
			      struct seg6_local_lwt *slwt,
			      struct netlink_ext_ack *extack)
{
	struct seg6_end_dt_info *info = seg6_possible_end_dt_info(slwt);

	if (IS_ERR(info))
		return PTR_ERR(info);

	info->vrf_table = nla_get_u32(attrs[SEG6_LOCAL_VRFTABLE]);

	return 0;
}

static int put_nla_vrftable(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct seg6_end_dt_info *info = seg6_possible_end_dt_info(slwt);

	if (IS_ERR(info))
		return PTR_ERR(info);

	if (nla_put_u32(skb, SEG6_LOCAL_VRFTABLE, info->vrf_table))
		return -EMSGSIZE;

	return 0;
}

static int cmp_nla_vrftable(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	struct seg6_end_dt_info *info_a = seg6_possible_end_dt_info(a);
	struct seg6_end_dt_info *info_b = seg6_possible_end_dt_info(b);

	if (info_a->vrf_table != info_b->vrf_table)
		return 1;

	return 0;
}

static int parse_nla_nh4(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			 struct netlink_ext_ack *extack)
{
	memcpy(&slwt->nh4, nla_data(attrs[SEG6_LOCAL_NH4]),
	       sizeof(struct in_addr));

	return 0;
}

static int put_nla_nh4(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct nlattr *nla;

	nla = nla_reserve(skb, SEG6_LOCAL_NH4, sizeof(struct in_addr));
	if (!nla)
		return -EMSGSIZE;

	memcpy(nla_data(nla), &slwt->nh4, sizeof(struct in_addr));

	return 0;
}

static int cmp_nla_nh4(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	return memcmp(&a->nh4, &b->nh4, sizeof(struct in_addr));
}

static int parse_nla_nh6(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			 struct netlink_ext_ack *extack)
{
	memcpy(&slwt->nh6, nla_data(attrs[SEG6_LOCAL_NH6]),
	       sizeof(struct in6_addr));

	return 0;
}

static int put_nla_nh6(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct nlattr *nla;

	nla = nla_reserve(skb, SEG6_LOCAL_NH6, sizeof(struct in6_addr));
	if (!nla)
		return -EMSGSIZE;

	memcpy(nla_data(nla), &slwt->nh6, sizeof(struct in6_addr));

	return 0;
}

static int cmp_nla_nh6(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	return memcmp(&a->nh6, &b->nh6, sizeof(struct in6_addr));
}

static int parse_nla_iif(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			 struct netlink_ext_ack *extack)
{
	slwt->iif = nla_get_u32(attrs[SEG6_LOCAL_IIF]);

	return 0;
}

static int put_nla_iif(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	if (nla_put_u32(skb, SEG6_LOCAL_IIF, slwt->iif))
		return -EMSGSIZE;

	return 0;
}

static int cmp_nla_iif(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	if (a->iif != b->iif)
		return 1;

	return 0;
}

static int parse_nla_oif(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			 struct netlink_ext_ack *extack)
{
	slwt->oif = nla_get_u32(attrs[SEG6_LOCAL_OIF]);

	return 0;
}

static int put_nla_oif(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	if (nla_put_u32(skb, SEG6_LOCAL_OIF, slwt->oif))
		return -EMSGSIZE;

	return 0;
}

static int cmp_nla_oif(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	if (a->oif != b->oif)
		return 1;

	return 0;
}

#define MAX_PROG_NAME 256
static const struct nla_policy bpf_prog_policy[SEG6_LOCAL_BPF_PROG_MAX + 1] = {
	[SEG6_LOCAL_BPF_PROG]	   = { .type = NLA_U32, },
	[SEG6_LOCAL_BPF_PROG_NAME] = { .type = NLA_NUL_STRING,
				       .len = MAX_PROG_NAME },
};

static int parse_nla_bpf(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			 struct netlink_ext_ack *extack)
{
	struct nlattr *tb[SEG6_LOCAL_BPF_PROG_MAX + 1];
	struct bpf_prog *p;
	int ret;
	u32 fd;

	ret = nla_parse_nested_deprecated(tb, SEG6_LOCAL_BPF_PROG_MAX,
					  attrs[SEG6_LOCAL_BPF],
					  bpf_prog_policy, NULL);
	if (ret < 0)
		return ret;

	if (!tb[SEG6_LOCAL_BPF_PROG] || !tb[SEG6_LOCAL_BPF_PROG_NAME])
		return -EINVAL;

	slwt->bpf.name = nla_memdup(tb[SEG6_LOCAL_BPF_PROG_NAME], GFP_KERNEL);
	if (!slwt->bpf.name)
		return -ENOMEM;

	fd = nla_get_u32(tb[SEG6_LOCAL_BPF_PROG]);
	p = bpf_prog_get_type(fd, BPF_PROG_TYPE_LWT_SEG6LOCAL);
	if (IS_ERR(p)) {
		kfree(slwt->bpf.name);
		return PTR_ERR(p);
	}

	slwt->bpf.prog = p;
	return 0;
}

static int put_nla_bpf(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct nlattr *nest;

	if (!slwt->bpf.prog)
		return 0;

	nest = nla_nest_start_noflag(skb, SEG6_LOCAL_BPF);
	if (!nest)
		return -EMSGSIZE;

	if (nla_put_u32(skb, SEG6_LOCAL_BPF_PROG, slwt->bpf.prog->aux->id))
		return -EMSGSIZE;

	if (slwt->bpf.name &&
	    nla_put_string(skb, SEG6_LOCAL_BPF_PROG_NAME, slwt->bpf.name))
		return -EMSGSIZE;

	return nla_nest_end(skb, nest);
}

static int cmp_nla_bpf(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	if (!a->bpf.name && !b->bpf.name)
		return 0;

	if (!a->bpf.name || !b->bpf.name)
		return 1;

	return strcmp(a->bpf.name, b->bpf.name);
}

static void destroy_attr_bpf(struct seg6_local_lwt *slwt)
{
	kfree(slwt->bpf.name);
	if (slwt->bpf.prog)
		bpf_prog_put(slwt->bpf.prog);
}

static const struct
nla_policy seg6_local_counters_policy[SEG6_LOCAL_CNT_MAX + 1] = {
	[SEG6_LOCAL_CNT_PACKETS]	= { .type = NLA_U64 },
	[SEG6_LOCAL_CNT_BYTES]		= { .type = NLA_U64 },
	[SEG6_LOCAL_CNT_ERRORS]		= { .type = NLA_U64 },
};

static int parse_nla_counters(struct nlattr **attrs,
			      struct seg6_local_lwt *slwt,
			      struct netlink_ext_ack *extack)
{
	struct pcpu_seg6_local_counters __percpu *pcounters;
	struct nlattr *tb[SEG6_LOCAL_CNT_MAX + 1];
	int ret;

	ret = nla_parse_nested_deprecated(tb, SEG6_LOCAL_CNT_MAX,
					  attrs[SEG6_LOCAL_COUNTERS],
					  seg6_local_counters_policy, NULL);
	if (ret < 0)
		return ret;

	 
	if (!tb[SEG6_LOCAL_CNT_PACKETS] || !tb[SEG6_LOCAL_CNT_BYTES] ||
	    !tb[SEG6_LOCAL_CNT_ERRORS])
		return -EINVAL;

	 
	pcounters = seg6_local_alloc_pcpu_counters(GFP_KERNEL);
	if (!pcounters)
		return -ENOMEM;

	slwt->pcpu_counters = pcounters;

	return 0;
}

static int seg6_local_fill_nla_counters(struct sk_buff *skb,
					struct seg6_local_counters *counters)
{
	if (nla_put_u64_64bit(skb, SEG6_LOCAL_CNT_PACKETS, counters->packets,
			      SEG6_LOCAL_CNT_PAD))
		return -EMSGSIZE;

	if (nla_put_u64_64bit(skb, SEG6_LOCAL_CNT_BYTES, counters->bytes,
			      SEG6_LOCAL_CNT_PAD))
		return -EMSGSIZE;

	if (nla_put_u64_64bit(skb, SEG6_LOCAL_CNT_ERRORS, counters->errors,
			      SEG6_LOCAL_CNT_PAD))
		return -EMSGSIZE;

	return 0;
}

static int put_nla_counters(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct seg6_local_counters counters = { 0, 0, 0 };
	struct nlattr *nest;
	int rc, i;

	nest = nla_nest_start(skb, SEG6_LOCAL_COUNTERS);
	if (!nest)
		return -EMSGSIZE;

	for_each_possible_cpu(i) {
		struct pcpu_seg6_local_counters *pcounters;
		u64 packets, bytes, errors;
		unsigned int start;

		pcounters = per_cpu_ptr(slwt->pcpu_counters, i);
		do {
			start = u64_stats_fetch_begin(&pcounters->syncp);

			packets = u64_stats_read(&pcounters->packets);
			bytes = u64_stats_read(&pcounters->bytes);
			errors = u64_stats_read(&pcounters->errors);

		} while (u64_stats_fetch_retry(&pcounters->syncp, start));

		counters.packets += packets;
		counters.bytes += bytes;
		counters.errors += errors;
	}

	rc = seg6_local_fill_nla_counters(skb, &counters);
	if (rc < 0) {
		nla_nest_cancel(skb, nest);
		return rc;
	}

	return nla_nest_end(skb, nest);
}

static int cmp_nla_counters(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	 
	return (!!((unsigned long)a->pcpu_counters)) ^
		(!!((unsigned long)b->pcpu_counters));
}

static void destroy_attr_counters(struct seg6_local_lwt *slwt)
{
	free_percpu(slwt->pcpu_counters);
}

static const
struct nla_policy seg6_local_flavors_policy[SEG6_LOCAL_FLV_MAX + 1] = {
	[SEG6_LOCAL_FLV_OPERATION]	= { .type = NLA_U32 },
	[SEG6_LOCAL_FLV_LCBLOCK_BITS]	= { .type = NLA_U8 },
	[SEG6_LOCAL_FLV_LCNODE_FN_BITS]	= { .type = NLA_U8 },
};

 
static int seg6_chk_next_csid_cfg(__u8 block_len, __u8 func_len)
{
	 
	if (next_csid_chk_cntr_bits(block_len, func_len))
		return -EINVAL;

	 
	if (next_csid_chk_lcblock_bits(block_len))
		return -EINVAL;

	 
	if (next_csid_chk_lcnode_fn_bits(func_len))
		return -EINVAL;

	return 0;
}

static int seg6_parse_nla_next_csid_cfg(struct nlattr **tb,
					struct seg6_flavors_info *finfo,
					struct netlink_ext_ack *extack)
{
	__u8 func_len = SEG6_LOCAL_LCNODE_FN_DBITS;
	__u8 block_len = SEG6_LOCAL_LCBLOCK_DBITS;
	int rc;

	if (tb[SEG6_LOCAL_FLV_LCBLOCK_BITS])
		block_len = nla_get_u8(tb[SEG6_LOCAL_FLV_LCBLOCK_BITS]);

	if (tb[SEG6_LOCAL_FLV_LCNODE_FN_BITS])
		func_len = nla_get_u8(tb[SEG6_LOCAL_FLV_LCNODE_FN_BITS]);

	rc = seg6_chk_next_csid_cfg(block_len, func_len);
	if (rc < 0) {
		NL_SET_ERR_MSG(extack,
			       "Invalid Locator Block/Node Function lengths");
		return rc;
	}

	finfo->lcblock_bits = block_len;
	finfo->lcnode_func_bits = func_len;

	return 0;
}

static int parse_nla_flavors(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			     struct netlink_ext_ack *extack)
{
	struct seg6_flavors_info *finfo = &slwt->flv_info;
	struct nlattr *tb[SEG6_LOCAL_FLV_MAX + 1];
	int action = slwt->action;
	__u32 fops, supp_fops;
	int rc;

	rc = nla_parse_nested_deprecated(tb, SEG6_LOCAL_FLV_MAX,
					 attrs[SEG6_LOCAL_FLAVORS],
					 seg6_local_flavors_policy, NULL);
	if (rc < 0)
		return rc;

	 
	if (!tb[SEG6_LOCAL_FLV_OPERATION])
		return -EINVAL;

	fops = nla_get_u32(tb[SEG6_LOCAL_FLV_OPERATION]);
	rc = seg6_flv_supp_ops_by_action(action, &supp_fops);
	if (rc < 0 || (fops & ~supp_fops)) {
		NL_SET_ERR_MSG(extack, "Unsupported Flavor operation(s)");
		return -EOPNOTSUPP;
	}

	finfo->flv_ops = fops;

	if (seg6_next_csid_enabled(fops)) {
		 
		rc = seg6_parse_nla_next_csid_cfg(tb, finfo, extack);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int seg6_fill_nla_next_csid_cfg(struct sk_buff *skb,
				       struct seg6_flavors_info *finfo)
{
	if (nla_put_u8(skb, SEG6_LOCAL_FLV_LCBLOCK_BITS, finfo->lcblock_bits))
		return -EMSGSIZE;

	if (nla_put_u8(skb, SEG6_LOCAL_FLV_LCNODE_FN_BITS,
		       finfo->lcnode_func_bits))
		return -EMSGSIZE;

	return 0;
}

static int put_nla_flavors(struct sk_buff *skb, struct seg6_local_lwt *slwt)
{
	struct seg6_flavors_info *finfo = &slwt->flv_info;
	__u32 fops = finfo->flv_ops;
	struct nlattr *nest;
	int rc;

	nest = nla_nest_start(skb, SEG6_LOCAL_FLAVORS);
	if (!nest)
		return -EMSGSIZE;

	if (nla_put_u32(skb, SEG6_LOCAL_FLV_OPERATION, fops)) {
		rc = -EMSGSIZE;
		goto err;
	}

	if (seg6_next_csid_enabled(fops)) {
		rc = seg6_fill_nla_next_csid_cfg(skb, finfo);
		if (rc < 0)
			goto err;
	}

	return nla_nest_end(skb, nest);

err:
	nla_nest_cancel(skb, nest);
	return rc;
}

static int seg6_cmp_nla_next_csid_cfg(struct seg6_flavors_info *finfo_a,
				      struct seg6_flavors_info *finfo_b)
{
	if (finfo_a->lcblock_bits != finfo_b->lcblock_bits)
		return 1;

	if (finfo_a->lcnode_func_bits != finfo_b->lcnode_func_bits)
		return 1;

	return 0;
}

static int cmp_nla_flavors(struct seg6_local_lwt *a, struct seg6_local_lwt *b)
{
	struct seg6_flavors_info *finfo_a = &a->flv_info;
	struct seg6_flavors_info *finfo_b = &b->flv_info;

	if (finfo_a->flv_ops != finfo_b->flv_ops)
		return 1;

	if (seg6_next_csid_enabled(finfo_a->flv_ops)) {
		if (seg6_cmp_nla_next_csid_cfg(finfo_a, finfo_b))
			return 1;
	}

	return 0;
}

static int encap_size_flavors(struct seg6_local_lwt *slwt)
{
	struct seg6_flavors_info *finfo = &slwt->flv_info;
	int nlsize;

	nlsize = nla_total_size(0) +	 
		 nla_total_size(4);	 

	if (seg6_next_csid_enabled(finfo->flv_ops))
		nlsize += nla_total_size(1) +  
			  nla_total_size(1);  

	return nlsize;
}

struct seg6_action_param {
	int (*parse)(struct nlattr **attrs, struct seg6_local_lwt *slwt,
		     struct netlink_ext_ack *extack);
	int (*put)(struct sk_buff *skb, struct seg6_local_lwt *slwt);
	int (*cmp)(struct seg6_local_lwt *a, struct seg6_local_lwt *b);

	 
	void (*destroy)(struct seg6_local_lwt *slwt);
};

static struct seg6_action_param seg6_action_params[SEG6_LOCAL_MAX + 1] = {
	[SEG6_LOCAL_SRH]	= { .parse = parse_nla_srh,
				    .put = put_nla_srh,
				    .cmp = cmp_nla_srh,
				    .destroy = destroy_attr_srh },

	[SEG6_LOCAL_TABLE]	= { .parse = parse_nla_table,
				    .put = put_nla_table,
				    .cmp = cmp_nla_table },

	[SEG6_LOCAL_NH4]	= { .parse = parse_nla_nh4,
				    .put = put_nla_nh4,
				    .cmp = cmp_nla_nh4 },

	[SEG6_LOCAL_NH6]	= { .parse = parse_nla_nh6,
				    .put = put_nla_nh6,
				    .cmp = cmp_nla_nh6 },

	[SEG6_LOCAL_IIF]	= { .parse = parse_nla_iif,
				    .put = put_nla_iif,
				    .cmp = cmp_nla_iif },

	[SEG6_LOCAL_OIF]	= { .parse = parse_nla_oif,
				    .put = put_nla_oif,
				    .cmp = cmp_nla_oif },

	[SEG6_LOCAL_BPF]	= { .parse = parse_nla_bpf,
				    .put = put_nla_bpf,
				    .cmp = cmp_nla_bpf,
				    .destroy = destroy_attr_bpf },

	[SEG6_LOCAL_VRFTABLE]	= { .parse = parse_nla_vrftable,
				    .put = put_nla_vrftable,
				    .cmp = cmp_nla_vrftable },

	[SEG6_LOCAL_COUNTERS]	= { .parse = parse_nla_counters,
				    .put = put_nla_counters,
				    .cmp = cmp_nla_counters,
				    .destroy = destroy_attr_counters },

	[SEG6_LOCAL_FLAVORS]	= { .parse = parse_nla_flavors,
				    .put = put_nla_flavors,
				    .cmp = cmp_nla_flavors },
};

 
static void __destroy_attrs(unsigned long parsed_attrs, int max_parsed,
			    struct seg6_local_lwt *slwt)
{
	struct seg6_action_param *param;
	int i;

	 
	for (i = SEG6_LOCAL_SRH; i < max_parsed; ++i) {
		if (!(parsed_attrs & SEG6_F_ATTR(i)))
			continue;

		param = &seg6_action_params[i];

		if (param->destroy)
			param->destroy(slwt);
	}
}

 
static void destroy_attrs(struct seg6_local_lwt *slwt)
{
	unsigned long attrs = slwt->desc->attrs | slwt->parsed_optattrs;

	__destroy_attrs(attrs, SEG6_LOCAL_MAX + 1, slwt);
}

static int parse_nla_optional_attrs(struct nlattr **attrs,
				    struct seg6_local_lwt *slwt,
				    struct netlink_ext_ack *extack)
{
	struct seg6_action_desc *desc = slwt->desc;
	unsigned long parsed_optattrs = 0;
	struct seg6_action_param *param;
	int err, i;

	for (i = SEG6_LOCAL_SRH; i < SEG6_LOCAL_MAX + 1; ++i) {
		if (!(desc->optattrs & SEG6_F_ATTR(i)) || !attrs[i])
			continue;

		 
		param = &seg6_action_params[i];

		err = param->parse(attrs, slwt, extack);
		if (err < 0)
			goto parse_optattrs_err;

		 
		parsed_optattrs |= SEG6_F_ATTR(i);
	}

	 
	slwt->parsed_optattrs = parsed_optattrs;

	return 0;

parse_optattrs_err:
	__destroy_attrs(parsed_optattrs, i, slwt);

	return err;
}

 
static int
seg6_local_lwtunnel_build_state(struct seg6_local_lwt *slwt, const void *cfg,
				struct netlink_ext_ack *extack)
{
	struct seg6_action_desc *desc = slwt->desc;
	struct seg6_local_lwtunnel_ops *ops;

	ops = &desc->slwt_ops;
	if (!ops->build_state)
		return 0;

	return ops->build_state(slwt, cfg, extack);
}

 
static void seg6_local_lwtunnel_destroy_state(struct seg6_local_lwt *slwt)
{
	struct seg6_action_desc *desc = slwt->desc;
	struct seg6_local_lwtunnel_ops *ops;

	ops = &desc->slwt_ops;
	if (!ops->destroy_state)
		return;

	ops->destroy_state(slwt);
}

static int parse_nla_action(struct nlattr **attrs, struct seg6_local_lwt *slwt,
			    struct netlink_ext_ack *extack)
{
	struct seg6_action_param *param;
	struct seg6_action_desc *desc;
	unsigned long invalid_attrs;
	int i, err;

	desc = __get_action_desc(slwt->action);
	if (!desc)
		return -EINVAL;

	if (!desc->input)
		return -EOPNOTSUPP;

	slwt->desc = desc;
	slwt->headroom += desc->static_headroom;

	 
	invalid_attrs = desc->attrs & desc->optattrs;
	if (invalid_attrs) {
		WARN_ONCE(1,
			  "An attribute cannot be both required AND optional");
		return -EINVAL;
	}

	 
	for (i = SEG6_LOCAL_SRH; i < SEG6_LOCAL_MAX + 1; i++) {
		if (desc->attrs & SEG6_F_ATTR(i)) {
			if (!attrs[i])
				return -EINVAL;

			param = &seg6_action_params[i];

			err = param->parse(attrs, slwt, extack);
			if (err < 0)
				goto parse_attrs_err;
		}
	}

	 
	err = parse_nla_optional_attrs(attrs, slwt, extack);
	if (err < 0)
		goto parse_attrs_err;

	return 0;

parse_attrs_err:
	 
	__destroy_attrs(desc->attrs, i, slwt);

	return err;
}

static int seg6_local_build_state(struct net *net, struct nlattr *nla,
				  unsigned int family, const void *cfg,
				  struct lwtunnel_state **ts,
				  struct netlink_ext_ack *extack)
{
	struct nlattr *tb[SEG6_LOCAL_MAX + 1];
	struct lwtunnel_state *newts;
	struct seg6_local_lwt *slwt;
	int err;

	if (family != AF_INET6)
		return -EINVAL;

	err = nla_parse_nested_deprecated(tb, SEG6_LOCAL_MAX, nla,
					  seg6_local_policy, extack);

	if (err < 0)
		return err;

	if (!tb[SEG6_LOCAL_ACTION])
		return -EINVAL;

	newts = lwtunnel_state_alloc(sizeof(*slwt));
	if (!newts)
		return -ENOMEM;

	slwt = seg6_local_lwtunnel(newts);
	slwt->action = nla_get_u32(tb[SEG6_LOCAL_ACTION]);

	err = parse_nla_action(tb, slwt, extack);
	if (err < 0)
		goto out_free;

	err = seg6_local_lwtunnel_build_state(slwt, cfg, extack);
	if (err < 0)
		goto out_destroy_attrs;

	newts->type = LWTUNNEL_ENCAP_SEG6_LOCAL;
	newts->flags = LWTUNNEL_STATE_INPUT_REDIRECT;
	newts->headroom = slwt->headroom;

	*ts = newts;

	return 0;

out_destroy_attrs:
	destroy_attrs(slwt);
out_free:
	kfree(newts);
	return err;
}

static void seg6_local_destroy_state(struct lwtunnel_state *lwt)
{
	struct seg6_local_lwt *slwt = seg6_local_lwtunnel(lwt);

	seg6_local_lwtunnel_destroy_state(slwt);

	destroy_attrs(slwt);

	return;
}

static int seg6_local_fill_encap(struct sk_buff *skb,
				 struct lwtunnel_state *lwt)
{
	struct seg6_local_lwt *slwt = seg6_local_lwtunnel(lwt);
	struct seg6_action_param *param;
	unsigned long attrs;
	int i, err;

	if (nla_put_u32(skb, SEG6_LOCAL_ACTION, slwt->action))
		return -EMSGSIZE;

	attrs = slwt->desc->attrs | slwt->parsed_optattrs;

	for (i = SEG6_LOCAL_SRH; i < SEG6_LOCAL_MAX + 1; i++) {
		if (attrs & SEG6_F_ATTR(i)) {
			param = &seg6_action_params[i];
			err = param->put(skb, slwt);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

static int seg6_local_get_encap_size(struct lwtunnel_state *lwt)
{
	struct seg6_local_lwt *slwt = seg6_local_lwtunnel(lwt);
	unsigned long attrs;
	int nlsize;

	nlsize = nla_total_size(4);  

	attrs = slwt->desc->attrs | slwt->parsed_optattrs;

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_SRH))
		nlsize += nla_total_size((slwt->srh->hdrlen + 1) << 3);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_TABLE))
		nlsize += nla_total_size(4);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_NH4))
		nlsize += nla_total_size(4);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_NH6))
		nlsize += nla_total_size(16);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_IIF))
		nlsize += nla_total_size(4);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_OIF))
		nlsize += nla_total_size(4);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_BPF))
		nlsize += nla_total_size(sizeof(struct nlattr)) +
		       nla_total_size(MAX_PROG_NAME) +
		       nla_total_size(4);

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_VRFTABLE))
		nlsize += nla_total_size(4);

	if (attrs & SEG6_F_LOCAL_COUNTERS)
		nlsize += nla_total_size(0) +  
			   
			  nla_total_size_64bit(sizeof(__u64)) +
			   
			  nla_total_size_64bit(sizeof(__u64)) +
			   
			  nla_total_size_64bit(sizeof(__u64));

	if (attrs & SEG6_F_ATTR(SEG6_LOCAL_FLAVORS))
		nlsize += encap_size_flavors(slwt);

	return nlsize;
}

static int seg6_local_cmp_encap(struct lwtunnel_state *a,
				struct lwtunnel_state *b)
{
	struct seg6_local_lwt *slwt_a, *slwt_b;
	struct seg6_action_param *param;
	unsigned long attrs_a, attrs_b;
	int i;

	slwt_a = seg6_local_lwtunnel(a);
	slwt_b = seg6_local_lwtunnel(b);

	if (slwt_a->action != slwt_b->action)
		return 1;

	attrs_a = slwt_a->desc->attrs | slwt_a->parsed_optattrs;
	attrs_b = slwt_b->desc->attrs | slwt_b->parsed_optattrs;

	if (attrs_a != attrs_b)
		return 1;

	for (i = SEG6_LOCAL_SRH; i < SEG6_LOCAL_MAX + 1; i++) {
		if (attrs_a & SEG6_F_ATTR(i)) {
			param = &seg6_action_params[i];
			if (param->cmp(slwt_a, slwt_b))
				return 1;
		}
	}

	return 0;
}

static const struct lwtunnel_encap_ops seg6_local_ops = {
	.build_state	= seg6_local_build_state,
	.destroy_state	= seg6_local_destroy_state,
	.input		= seg6_local_input,
	.fill_encap	= seg6_local_fill_encap,
	.get_encap_size	= seg6_local_get_encap_size,
	.cmp_encap	= seg6_local_cmp_encap,
	.owner		= THIS_MODULE,
};

int __init seg6_local_init(void)
{
	 
	BUILD_BUG_ON(SEG6_LOCAL_MAX + 1 > BITS_PER_TYPE(unsigned long));

	 
	BUILD_BUG_ON(SEG6_LOCAL_FLV_OP_MAX + 1 > BITS_PER_TYPE(__u32));

	 
	BUILD_BUG_ON(next_csid_chk_cntr_bits(SEG6_LOCAL_LCBLOCK_DBITS,
					     SEG6_LOCAL_LCNODE_FN_DBITS));
	BUILD_BUG_ON(next_csid_chk_lcblock_bits(SEG6_LOCAL_LCBLOCK_DBITS));
	BUILD_BUG_ON(next_csid_chk_lcnode_fn_bits(SEG6_LOCAL_LCNODE_FN_DBITS));

	 
	BUILD_BUG_ON(SEG6_LOCAL_FLV_ACT_MAX > (typeof(flv8986_act_tbl[0]))~0U);

	return lwtunnel_encap_add_ops(&seg6_local_ops,
				      LWTUNNEL_ENCAP_SEG6_LOCAL);
}

void seg6_local_exit(void)
{
	lwtunnel_encap_del_ops(&seg6_local_ops, LWTUNNEL_ENCAP_SEG6_LOCAL);
}
