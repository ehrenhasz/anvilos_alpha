
 

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/netfilter.h>
#include <linux/in.h>
#include <linux/icmp.h>
#include <linux/seq_file.h>
#include <net/ip.h>
#include <net/checksum.h>
#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_timeout.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_log.h>

#include "nf_internals.h"

static const unsigned int nf_ct_icmp_timeout = 30*HZ;

bool icmp_pkt_to_tuple(const struct sk_buff *skb, unsigned int dataoff,
		       struct net *net, struct nf_conntrack_tuple *tuple)
{
	const struct icmphdr *hp;
	struct icmphdr _hdr;

	hp = skb_header_pointer(skb, dataoff, sizeof(_hdr), &_hdr);
	if (hp == NULL)
		return false;

	tuple->dst.u.icmp.type = hp->type;
	tuple->src.u.icmp.id = hp->un.echo.id;
	tuple->dst.u.icmp.code = hp->code;

	return true;
}

 
static const u_int8_t invmap[] = {
	[ICMP_ECHO] = ICMP_ECHOREPLY + 1,
	[ICMP_ECHOREPLY] = ICMP_ECHO + 1,
	[ICMP_TIMESTAMP] = ICMP_TIMESTAMPREPLY + 1,
	[ICMP_TIMESTAMPREPLY] = ICMP_TIMESTAMP + 1,
	[ICMP_INFO_REQUEST] = ICMP_INFO_REPLY + 1,
	[ICMP_INFO_REPLY] = ICMP_INFO_REQUEST + 1,
	[ICMP_ADDRESS] = ICMP_ADDRESSREPLY + 1,
	[ICMP_ADDRESSREPLY] = ICMP_ADDRESS + 1
};

bool nf_conntrack_invert_icmp_tuple(struct nf_conntrack_tuple *tuple,
				    const struct nf_conntrack_tuple *orig)
{
	if (orig->dst.u.icmp.type >= sizeof(invmap) ||
	    !invmap[orig->dst.u.icmp.type])
		return false;

	tuple->src.u.icmp.id = orig->src.u.icmp.id;
	tuple->dst.u.icmp.type = invmap[orig->dst.u.icmp.type] - 1;
	tuple->dst.u.icmp.code = orig->dst.u.icmp.code;
	return true;
}

 
int nf_conntrack_icmp_packet(struct nf_conn *ct,
			     struct sk_buff *skb,
			     enum ip_conntrack_info ctinfo,
			     const struct nf_hook_state *state)
{
	 
	unsigned int *timeout = nf_ct_timeout_lookup(ct);
	static const u_int8_t valid_new[] = {
		[ICMP_ECHO] = 1,
		[ICMP_TIMESTAMP] = 1,
		[ICMP_INFO_REQUEST] = 1,
		[ICMP_ADDRESS] = 1
	};

	if (state->pf != NFPROTO_IPV4)
		return -NF_ACCEPT;

	if (ct->tuplehash[0].tuple.dst.u.icmp.type >= sizeof(valid_new) ||
	    !valid_new[ct->tuplehash[0].tuple.dst.u.icmp.type]) {
		 
		pr_debug("icmp: can't create new conn with type %u\n",
			 ct->tuplehash[0].tuple.dst.u.icmp.type);
		nf_ct_dump_tuple_ip(&ct->tuplehash[0].tuple);
		return -NF_ACCEPT;
	}

	if (!timeout)
		timeout = &nf_icmp_pernet(nf_ct_net(ct))->timeout;

	nf_ct_refresh_acct(ct, ctinfo, skb, *timeout);
	return NF_ACCEPT;
}

 
int nf_conntrack_inet_error(struct nf_conn *tmpl, struct sk_buff *skb,
			    unsigned int dataoff,
			    const struct nf_hook_state *state,
			    u8 l4proto, union nf_inet_addr *outer_daddr)
{
	struct nf_conntrack_tuple innertuple, origtuple;
	const struct nf_conntrack_tuple_hash *h;
	const struct nf_conntrack_zone *zone;
	enum ip_conntrack_info ctinfo;
	struct nf_conntrack_zone tmp;
	union nf_inet_addr *ct_daddr;
	enum ip_conntrack_dir dir;
	struct nf_conn *ct;

	WARN_ON(skb_nfct(skb));
	zone = nf_ct_zone_tmpl(tmpl, skb, &tmp);

	 
	if (!nf_ct_get_tuplepr(skb, dataoff,
			       state->pf, state->net, &origtuple))
		return -NF_ACCEPT;

	 
	if (!nf_ct_invert_tuple(&innertuple, &origtuple))
		return -NF_ACCEPT;

	h = nf_conntrack_find_get(state->net, zone, &innertuple);
	if (!h)
		return -NF_ACCEPT;

	 
	ct = nf_ct_tuplehash_to_ctrack(h);
	dir = NF_CT_DIRECTION(h);
	ct_daddr = &ct->tuplehash[dir].tuple.dst.u3;
	if (!nf_inet_addr_cmp(outer_daddr, ct_daddr)) {
		if (state->pf == AF_INET) {
			nf_l4proto_log_invalid(skb, state,
					       l4proto,
					       "outer daddr %pI4 != inner %pI4",
					       &outer_daddr->ip, &ct_daddr->ip);
		} else if (state->pf == AF_INET6) {
			nf_l4proto_log_invalid(skb, state,
					       l4proto,
					       "outer daddr %pI6 != inner %pI6",
					       &outer_daddr->ip6, &ct_daddr->ip6);
		}
		nf_ct_put(ct);
		return -NF_ACCEPT;
	}

	ctinfo = IP_CT_RELATED;
	if (dir == IP_CT_DIR_REPLY)
		ctinfo += IP_CT_IS_REPLY;

	 
	nf_ct_set(skb, ct, ctinfo);
	return NF_ACCEPT;
}

static void icmp_error_log(const struct sk_buff *skb,
			   const struct nf_hook_state *state,
			   const char *msg)
{
	nf_l4proto_log_invalid(skb, state, IPPROTO_ICMP, "%s", msg);
}

 
int nf_conntrack_icmpv4_error(struct nf_conn *tmpl,
			      struct sk_buff *skb, unsigned int dataoff,
			      const struct nf_hook_state *state)
{
	union nf_inet_addr outer_daddr;
	const struct icmphdr *icmph;
	struct icmphdr _ih;

	 
	icmph = skb_header_pointer(skb, dataoff, sizeof(_ih), &_ih);
	if (icmph == NULL) {
		icmp_error_log(skb, state, "short packet");
		return -NF_ACCEPT;
	}

	 
	if (state->net->ct.sysctl_checksum &&
	    state->hook == NF_INET_PRE_ROUTING &&
	    nf_ip_checksum(skb, state->hook, dataoff, IPPROTO_ICMP)) {
		icmp_error_log(skb, state, "bad hw icmp checksum");
		return -NF_ACCEPT;
	}

	 
	if (icmph->type > NR_ICMP_TYPES) {
		icmp_error_log(skb, state, "invalid icmp type");
		return -NF_ACCEPT;
	}

	 
	if (!icmp_is_err(icmph->type))
		return NF_ACCEPT;

	memset(&outer_daddr, 0, sizeof(outer_daddr));
	outer_daddr.ip = ip_hdr(skb)->daddr;

	dataoff += sizeof(*icmph);
	return nf_conntrack_inet_error(tmpl, skb, dataoff, state,
				       IPPROTO_ICMP, &outer_daddr);
}

#if IS_ENABLED(CONFIG_NF_CT_NETLINK)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

static int icmp_tuple_to_nlattr(struct sk_buff *skb,
				const struct nf_conntrack_tuple *t)
{
	if (nla_put_be16(skb, CTA_PROTO_ICMP_ID, t->src.u.icmp.id) ||
	    nla_put_u8(skb, CTA_PROTO_ICMP_TYPE, t->dst.u.icmp.type) ||
	    nla_put_u8(skb, CTA_PROTO_ICMP_CODE, t->dst.u.icmp.code))
		goto nla_put_failure;
	return 0;

nla_put_failure:
	return -1;
}

static const struct nla_policy icmp_nla_policy[CTA_PROTO_MAX+1] = {
	[CTA_PROTO_ICMP_TYPE]	= { .type = NLA_U8 },
	[CTA_PROTO_ICMP_CODE]	= { .type = NLA_U8 },
	[CTA_PROTO_ICMP_ID]	= { .type = NLA_U16 },
};

static int icmp_nlattr_to_tuple(struct nlattr *tb[],
				struct nf_conntrack_tuple *tuple,
				u_int32_t flags)
{
	if (flags & CTA_FILTER_FLAG(CTA_PROTO_ICMP_TYPE)) {
		if (!tb[CTA_PROTO_ICMP_TYPE])
			return -EINVAL;

		tuple->dst.u.icmp.type = nla_get_u8(tb[CTA_PROTO_ICMP_TYPE]);
		if (tuple->dst.u.icmp.type >= sizeof(invmap) ||
		    !invmap[tuple->dst.u.icmp.type])
			return -EINVAL;
	}

	if (flags & CTA_FILTER_FLAG(CTA_PROTO_ICMP_CODE)) {
		if (!tb[CTA_PROTO_ICMP_CODE])
			return -EINVAL;

		tuple->dst.u.icmp.code = nla_get_u8(tb[CTA_PROTO_ICMP_CODE]);
	}

	if (flags & CTA_FILTER_FLAG(CTA_PROTO_ICMP_ID)) {
		if (!tb[CTA_PROTO_ICMP_ID])
			return -EINVAL;

		tuple->src.u.icmp.id = nla_get_be16(tb[CTA_PROTO_ICMP_ID]);
	}

	return 0;
}

static unsigned int icmp_nlattr_tuple_size(void)
{
	static unsigned int size __read_mostly;

	if (!size)
		size = nla_policy_len(icmp_nla_policy, CTA_PROTO_MAX + 1);

	return size;
}
#endif

#ifdef CONFIG_NF_CONNTRACK_TIMEOUT

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_cttimeout.h>

static int icmp_timeout_nlattr_to_obj(struct nlattr *tb[],
				      struct net *net, void *data)
{
	unsigned int *timeout = data;
	struct nf_icmp_net *in = nf_icmp_pernet(net);

	if (tb[CTA_TIMEOUT_ICMP_TIMEOUT]) {
		if (!timeout)
			timeout = &in->timeout;
		*timeout =
			ntohl(nla_get_be32(tb[CTA_TIMEOUT_ICMP_TIMEOUT])) * HZ;
	} else if (timeout) {
		 
		*timeout = in->timeout;
	}
	return 0;
}

static int
icmp_timeout_obj_to_nlattr(struct sk_buff *skb, const void *data)
{
	const unsigned int *timeout = data;

	if (nla_put_be32(skb, CTA_TIMEOUT_ICMP_TIMEOUT, htonl(*timeout / HZ)))
		goto nla_put_failure;
	return 0;

nla_put_failure:
	return -ENOSPC;
}

static const struct nla_policy
icmp_timeout_nla_policy[CTA_TIMEOUT_ICMP_MAX+1] = {
	[CTA_TIMEOUT_ICMP_TIMEOUT]	= { .type = NLA_U32 },
};
#endif  

void nf_conntrack_icmp_init_net(struct net *net)
{
	struct nf_icmp_net *in = nf_icmp_pernet(net);

	in->timeout = nf_ct_icmp_timeout;
}

const struct nf_conntrack_l4proto nf_conntrack_l4proto_icmp =
{
	.l4proto		= IPPROTO_ICMP,
#if IS_ENABLED(CONFIG_NF_CT_NETLINK)
	.tuple_to_nlattr	= icmp_tuple_to_nlattr,
	.nlattr_tuple_size	= icmp_nlattr_tuple_size,
	.nlattr_to_tuple	= icmp_nlattr_to_tuple,
	.nla_policy		= icmp_nla_policy,
#endif
#ifdef CONFIG_NF_CONNTRACK_TIMEOUT
	.ctnl_timeout		= {
		.nlattr_to_obj	= icmp_timeout_nlattr_to_obj,
		.obj_to_nlattr	= icmp_timeout_obj_to_nlattr,
		.nlattr_max	= CTA_TIMEOUT_ICMP_MAX,
		.obj_size	= sizeof(unsigned int),
		.nla_policy	= icmp_timeout_nla_policy,
	},
#endif  
};
