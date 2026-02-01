
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/dccp.h>
#include <linux/slab.h>

#include <net/net_namespace.h>
#include <net/netns/generic.h>

#include <linux/netfilter/nfnetlink_conntrack.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_timeout.h>
#include <net/netfilter/nf_log.h>

 

#define DCCP_MSL (2 * 60 * HZ)

#ifdef CONFIG_NF_CONNTRACK_PROCFS
static const char * const dccp_state_names[] = {
	[CT_DCCP_NONE]		= "NONE",
	[CT_DCCP_REQUEST]	= "REQUEST",
	[CT_DCCP_RESPOND]	= "RESPOND",
	[CT_DCCP_PARTOPEN]	= "PARTOPEN",
	[CT_DCCP_OPEN]		= "OPEN",
	[CT_DCCP_CLOSEREQ]	= "CLOSEREQ",
	[CT_DCCP_CLOSING]	= "CLOSING",
	[CT_DCCP_TIMEWAIT]	= "TIMEWAIT",
	[CT_DCCP_IGNORE]	= "IGNORE",
	[CT_DCCP_INVALID]	= "INVALID",
};
#endif

#define sNO	CT_DCCP_NONE
#define sRQ	CT_DCCP_REQUEST
#define sRS	CT_DCCP_RESPOND
#define sPO	CT_DCCP_PARTOPEN
#define sOP	CT_DCCP_OPEN
#define sCR	CT_DCCP_CLOSEREQ
#define sCG	CT_DCCP_CLOSING
#define sTW	CT_DCCP_TIMEWAIT
#define sIG	CT_DCCP_IGNORE
#define sIV	CT_DCCP_INVALID

 
static const u_int8_t
dccp_state_table[CT_DCCP_ROLE_MAX + 1][DCCP_PKT_SYNCACK + 1][CT_DCCP_MAX + 1] = {
	[CT_DCCP_ROLE_CLIENT] = {
		[DCCP_PKT_REQUEST] = {
		 
			sRQ, sRQ, sRS, sIG, sIG, sIG, sIG, sRQ,
		},
		[DCCP_PKT_RESPONSE] = {
		 
			sIV, sIG, sIG, sIG, sIG, sIG, sIG, sIV,
		},
		[DCCP_PKT_ACK] = {
		 
			sIV, sIV, sPO, sPO, sOP, sCR, sCG, sIV
		},
		[DCCP_PKT_DATA] = {
		 
			sIV, sIV, sIV, sIV, sOP, sCR, sCG, sIV,
		},
		[DCCP_PKT_DATAACK] = {
		 
			sIV, sIV, sPO, sPO, sOP, sCR, sCG, sIV
		},
		[DCCP_PKT_CLOSEREQ] = {
		 
			sIV, sIV, sIV, sIV, sIV, sIV, sIV, sIV
		},
		[DCCP_PKT_CLOSE] = {
		 
			sIV, sIV, sIV, sCG, sCG, sCG, sIV, sIV
		},
		[DCCP_PKT_RESET] = {
		 
			sIV, sTW, sTW, sTW, sTW, sTW, sTW, sIG
		},
		[DCCP_PKT_SYNC] = {
		 
			sIV, sIG, sIG, sIG, sIG, sIG, sIG, sIG,
		},
		[DCCP_PKT_SYNCACK] = {
		 
			sIV, sIG, sIG, sIG, sIG, sIG, sIG, sIG,
		},
	},
	[CT_DCCP_ROLE_SERVER] = {
		[DCCP_PKT_REQUEST] = {
		 
			sIV, sIG, sIG, sIG, sIG, sIG, sIG, sRQ
		},
		[DCCP_PKT_RESPONSE] = {
		 
			sIV, sRS, sRS, sIG, sIG, sIG, sIG, sIV
		},
		[DCCP_PKT_ACK] = {
		 
			sIV, sIV, sIV, sOP, sOP, sIV, sCG, sIV
		},
		[DCCP_PKT_DATA] = {
		 
			sIV, sIV, sIV, sOP, sOP, sIV, sCG, sIV
		},
		[DCCP_PKT_DATAACK] = {
		 
			sIV, sIV, sIV, sOP, sOP, sIV, sCG, sIV
		},
		[DCCP_PKT_CLOSEREQ] = {
		 
			sIV, sIV, sIV, sCR, sCR, sCR, sCR, sIV
		},
		[DCCP_PKT_CLOSE] = {
		 
			sIV, sIV, sIV, sCG, sCG, sIV, sCG, sIV
		},
		[DCCP_PKT_RESET] = {
		 
			sIV, sTW, sTW, sTW, sTW, sTW, sTW, sTW, sIG
		},
		[DCCP_PKT_SYNC] = {
		 
			sIV, sIG, sIG, sIG, sIG, sIG, sIG, sIG,
		},
		[DCCP_PKT_SYNCACK] = {
		 
			sIV, sIG, sIG, sIG, sIG, sIG, sIG, sIG,
		},
	},
};

static noinline bool
dccp_new(struct nf_conn *ct, const struct sk_buff *skb,
	 const struct dccp_hdr *dh,
	 const struct nf_hook_state *hook_state)
{
	struct net *net = nf_ct_net(ct);
	struct nf_dccp_net *dn;
	const char *msg;
	u_int8_t state;

	state = dccp_state_table[CT_DCCP_ROLE_CLIENT][dh->dccph_type][CT_DCCP_NONE];
	switch (state) {
	default:
		dn = nf_dccp_pernet(net);
		if (dn->dccp_loose == 0) {
			msg = "not picking up existing connection ";
			goto out_invalid;
		}
		break;
	case CT_DCCP_REQUEST:
		break;
	case CT_DCCP_INVALID:
		msg = "invalid state transition ";
		goto out_invalid;
	}

	ct->proto.dccp.role[IP_CT_DIR_ORIGINAL] = CT_DCCP_ROLE_CLIENT;
	ct->proto.dccp.role[IP_CT_DIR_REPLY] = CT_DCCP_ROLE_SERVER;
	ct->proto.dccp.state = CT_DCCP_NONE;
	ct->proto.dccp.last_pkt = DCCP_PKT_REQUEST;
	ct->proto.dccp.last_dir = IP_CT_DIR_ORIGINAL;
	ct->proto.dccp.handshake_seq = 0;
	return true;

out_invalid:
	nf_ct_l4proto_log_invalid(skb, ct, hook_state, "%s", msg);
	return false;
}

static u64 dccp_ack_seq(const struct dccp_hdr *dh)
{
	const struct dccp_hdr_ack_bits *dhack;

	dhack = (void *)dh + __dccp_basic_hdr_len(dh);
	return ((u64)ntohs(dhack->dccph_ack_nr_high) << 32) +
		     ntohl(dhack->dccph_ack_nr_low);
}

static bool dccp_error(const struct dccp_hdr *dh,
		       struct sk_buff *skb, unsigned int dataoff,
		       const struct nf_hook_state *state)
{
	static const unsigned long require_seq48 = 1 << DCCP_PKT_REQUEST |
						   1 << DCCP_PKT_RESPONSE |
						   1 << DCCP_PKT_CLOSEREQ |
						   1 << DCCP_PKT_CLOSE |
						   1 << DCCP_PKT_RESET |
						   1 << DCCP_PKT_SYNC |
						   1 << DCCP_PKT_SYNCACK;
	unsigned int dccp_len = skb->len - dataoff;
	unsigned int cscov;
	const char *msg;
	u8 type;

	BUILD_BUG_ON(DCCP_PKT_INVALID >= BITS_PER_LONG);

	if (dh->dccph_doff * 4 < sizeof(struct dccp_hdr) ||
	    dh->dccph_doff * 4 > dccp_len) {
		msg = "nf_ct_dccp: truncated/malformed packet ";
		goto out_invalid;
	}

	cscov = dccp_len;
	if (dh->dccph_cscov) {
		cscov = (dh->dccph_cscov - 1) * 4;
		if (cscov > dccp_len) {
			msg = "nf_ct_dccp: bad checksum coverage ";
			goto out_invalid;
		}
	}

	if (state->hook == NF_INET_PRE_ROUTING &&
	    state->net->ct.sysctl_checksum &&
	    nf_checksum_partial(skb, state->hook, dataoff, cscov,
				IPPROTO_DCCP, state->pf)) {
		msg = "nf_ct_dccp: bad checksum ";
		goto out_invalid;
	}

	type = dh->dccph_type;
	if (type >= DCCP_PKT_INVALID) {
		msg = "nf_ct_dccp: reserved packet type ";
		goto out_invalid;
	}

	if (test_bit(type, &require_seq48) && !dh->dccph_x) {
		msg = "nf_ct_dccp: type lacks 48bit sequence numbers";
		goto out_invalid;
	}

	return false;
out_invalid:
	nf_l4proto_log_invalid(skb, state, IPPROTO_DCCP, "%s", msg);
	return true;
}

struct nf_conntrack_dccp_buf {
	struct dccp_hdr dh;	  
	struct dccp_hdr_ext ext;  
	union {			  
		struct dccp_hdr_ack_bits ack;
		struct dccp_hdr_request req;
		struct dccp_hdr_response response;
		struct dccp_hdr_reset rst;
	} u;
};

static struct dccp_hdr *
dccp_header_pointer(const struct sk_buff *skb, int offset, const struct dccp_hdr *dh,
		    struct nf_conntrack_dccp_buf *buf)
{
	unsigned int hdrlen = __dccp_hdr_len(dh);

	if (hdrlen > sizeof(*buf))
		return NULL;

	return skb_header_pointer(skb, offset, hdrlen, buf);
}

int nf_conntrack_dccp_packet(struct nf_conn *ct, struct sk_buff *skb,
			     unsigned int dataoff,
			     enum ip_conntrack_info ctinfo,
			     const struct nf_hook_state *state)
{
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_dccp_buf _dh;
	u_int8_t type, old_state, new_state;
	enum ct_dccp_roles role;
	unsigned int *timeouts;
	struct dccp_hdr *dh;

	dh = skb_header_pointer(skb, dataoff, sizeof(*dh), &_dh.dh);
	if (!dh)
		return NF_DROP;

	if (dccp_error(dh, skb, dataoff, state))
		return -NF_ACCEPT;

	 
	dh = dccp_header_pointer(skb, dataoff, dh, &_dh);
	if (!dh)
		return NF_DROP;

	type = dh->dccph_type;
	if (!nf_ct_is_confirmed(ct) && !dccp_new(ct, skb, dh, state))
		return -NF_ACCEPT;

	if (type == DCCP_PKT_RESET &&
	    !test_bit(IPS_SEEN_REPLY_BIT, &ct->status)) {
		 
		nf_ct_kill_acct(ct, ctinfo, skb);
		return NF_ACCEPT;
	}

	spin_lock_bh(&ct->lock);

	role = ct->proto.dccp.role[dir];
	old_state = ct->proto.dccp.state;
	new_state = dccp_state_table[role][type][old_state];

	switch (new_state) {
	case CT_DCCP_REQUEST:
		if (old_state == CT_DCCP_TIMEWAIT &&
		    role == CT_DCCP_ROLE_SERVER) {
			 
			ct->proto.dccp.role[dir] = CT_DCCP_ROLE_CLIENT;
			ct->proto.dccp.role[!dir] = CT_DCCP_ROLE_SERVER;
		}
		break;
	case CT_DCCP_RESPOND:
		if (old_state == CT_DCCP_REQUEST)
			ct->proto.dccp.handshake_seq = dccp_hdr_seq(dh);
		break;
	case CT_DCCP_PARTOPEN:
		if (old_state == CT_DCCP_RESPOND &&
		    type == DCCP_PKT_ACK &&
		    dccp_ack_seq(dh) == ct->proto.dccp.handshake_seq)
			set_bit(IPS_ASSURED_BIT, &ct->status);
		break;
	case CT_DCCP_IGNORE:
		 
		if (ct->proto.dccp.last_dir == !dir &&
		    ct->proto.dccp.last_pkt == DCCP_PKT_REQUEST &&
		    type == DCCP_PKT_RESPONSE) {
			ct->proto.dccp.role[!dir] = CT_DCCP_ROLE_CLIENT;
			ct->proto.dccp.role[dir] = CT_DCCP_ROLE_SERVER;
			ct->proto.dccp.handshake_seq = dccp_hdr_seq(dh);
			new_state = CT_DCCP_RESPOND;
			break;
		}
		ct->proto.dccp.last_dir = dir;
		ct->proto.dccp.last_pkt = type;

		spin_unlock_bh(&ct->lock);
		nf_ct_l4proto_log_invalid(skb, ct, state, "%s", "invalid packet");
		return NF_ACCEPT;
	case CT_DCCP_INVALID:
		spin_unlock_bh(&ct->lock);
		nf_ct_l4proto_log_invalid(skb, ct, state, "%s", "invalid state transition");
		return -NF_ACCEPT;
	}

	ct->proto.dccp.last_dir = dir;
	ct->proto.dccp.last_pkt = type;
	ct->proto.dccp.state = new_state;
	spin_unlock_bh(&ct->lock);

	if (new_state != old_state)
		nf_conntrack_event_cache(IPCT_PROTOINFO, ct);

	timeouts = nf_ct_timeout_lookup(ct);
	if (!timeouts)
		timeouts = nf_dccp_pernet(nf_ct_net(ct))->dccp_timeout;
	nf_ct_refresh_acct(ct, ctinfo, skb, timeouts[new_state]);

	return NF_ACCEPT;
}

static bool dccp_can_early_drop(const struct nf_conn *ct)
{
	switch (ct->proto.dccp.state) {
	case CT_DCCP_CLOSEREQ:
	case CT_DCCP_CLOSING:
	case CT_DCCP_TIMEWAIT:
		return true;
	default:
		break;
	}

	return false;
}

#ifdef CONFIG_NF_CONNTRACK_PROCFS
static void dccp_print_conntrack(struct seq_file *s, struct nf_conn *ct)
{
	seq_printf(s, "%s ", dccp_state_names[ct->proto.dccp.state]);
}
#endif

#if IS_ENABLED(CONFIG_NF_CT_NETLINK)
static int dccp_to_nlattr(struct sk_buff *skb, struct nlattr *nla,
			  struct nf_conn *ct, bool destroy)
{
	struct nlattr *nest_parms;

	spin_lock_bh(&ct->lock);
	nest_parms = nla_nest_start(skb, CTA_PROTOINFO_DCCP);
	if (!nest_parms)
		goto nla_put_failure;
	if (nla_put_u8(skb, CTA_PROTOINFO_DCCP_STATE, ct->proto.dccp.state))
		goto nla_put_failure;

	if (destroy)
		goto skip_state;

	if (nla_put_u8(skb, CTA_PROTOINFO_DCCP_ROLE,
		       ct->proto.dccp.role[IP_CT_DIR_ORIGINAL]) ||
	    nla_put_be64(skb, CTA_PROTOINFO_DCCP_HANDSHAKE_SEQ,
			 cpu_to_be64(ct->proto.dccp.handshake_seq),
			 CTA_PROTOINFO_DCCP_PAD))
		goto nla_put_failure;
skip_state:
	nla_nest_end(skb, nest_parms);
	spin_unlock_bh(&ct->lock);

	return 0;

nla_put_failure:
	spin_unlock_bh(&ct->lock);
	return -1;
}

static const struct nla_policy dccp_nla_policy[CTA_PROTOINFO_DCCP_MAX + 1] = {
	[CTA_PROTOINFO_DCCP_STATE]	= { .type = NLA_U8 },
	[CTA_PROTOINFO_DCCP_ROLE]	= { .type = NLA_U8 },
	[CTA_PROTOINFO_DCCP_HANDSHAKE_SEQ] = { .type = NLA_U64 },
	[CTA_PROTOINFO_DCCP_PAD]	= { .type = NLA_UNSPEC },
};

#define DCCP_NLATTR_SIZE ( \
	NLA_ALIGN(NLA_HDRLEN + 1) + \
	NLA_ALIGN(NLA_HDRLEN + 1) + \
	NLA_ALIGN(NLA_HDRLEN + sizeof(u64)) + \
	NLA_ALIGN(NLA_HDRLEN + 0))

static int nlattr_to_dccp(struct nlattr *cda[], struct nf_conn *ct)
{
	struct nlattr *attr = cda[CTA_PROTOINFO_DCCP];
	struct nlattr *tb[CTA_PROTOINFO_DCCP_MAX + 1];
	int err;

	if (!attr)
		return 0;

	err = nla_parse_nested_deprecated(tb, CTA_PROTOINFO_DCCP_MAX, attr,
					  dccp_nla_policy, NULL);
	if (err < 0)
		return err;

	if (!tb[CTA_PROTOINFO_DCCP_STATE] ||
	    !tb[CTA_PROTOINFO_DCCP_ROLE] ||
	    nla_get_u8(tb[CTA_PROTOINFO_DCCP_ROLE]) > CT_DCCP_ROLE_MAX ||
	    nla_get_u8(tb[CTA_PROTOINFO_DCCP_STATE]) >= CT_DCCP_IGNORE) {
		return -EINVAL;
	}

	spin_lock_bh(&ct->lock);
	ct->proto.dccp.state = nla_get_u8(tb[CTA_PROTOINFO_DCCP_STATE]);
	if (nla_get_u8(tb[CTA_PROTOINFO_DCCP_ROLE]) == CT_DCCP_ROLE_CLIENT) {
		ct->proto.dccp.role[IP_CT_DIR_ORIGINAL] = CT_DCCP_ROLE_CLIENT;
		ct->proto.dccp.role[IP_CT_DIR_REPLY] = CT_DCCP_ROLE_SERVER;
	} else {
		ct->proto.dccp.role[IP_CT_DIR_ORIGINAL] = CT_DCCP_ROLE_SERVER;
		ct->proto.dccp.role[IP_CT_DIR_REPLY] = CT_DCCP_ROLE_CLIENT;
	}
	if (tb[CTA_PROTOINFO_DCCP_HANDSHAKE_SEQ]) {
		ct->proto.dccp.handshake_seq =
		be64_to_cpu(nla_get_be64(tb[CTA_PROTOINFO_DCCP_HANDSHAKE_SEQ]));
	}
	spin_unlock_bh(&ct->lock);
	return 0;
}
#endif

#ifdef CONFIG_NF_CONNTRACK_TIMEOUT

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_cttimeout.h>

static int dccp_timeout_nlattr_to_obj(struct nlattr *tb[],
				      struct net *net, void *data)
{
	struct nf_dccp_net *dn = nf_dccp_pernet(net);
	unsigned int *timeouts = data;
	int i;

	if (!timeouts)
		 timeouts = dn->dccp_timeout;

	 
	for (i=0; i<CT_DCCP_MAX; i++)
		timeouts[i] = dn->dccp_timeout[i];

	 
	for (i=CTA_TIMEOUT_DCCP_UNSPEC+1; i<CTA_TIMEOUT_DCCP_MAX+1; i++) {
		if (tb[i]) {
			timeouts[i] = ntohl(nla_get_be32(tb[i])) * HZ;
		}
	}

	timeouts[CTA_TIMEOUT_DCCP_UNSPEC] = timeouts[CTA_TIMEOUT_DCCP_REQUEST];
	return 0;
}

static int
dccp_timeout_obj_to_nlattr(struct sk_buff *skb, const void *data)
{
        const unsigned int *timeouts = data;
	int i;

	for (i=CTA_TIMEOUT_DCCP_UNSPEC+1; i<CTA_TIMEOUT_DCCP_MAX+1; i++) {
		if (nla_put_be32(skb, i, htonl(timeouts[i] / HZ)))
			goto nla_put_failure;
	}
	return 0;

nla_put_failure:
	return -ENOSPC;
}

static const struct nla_policy
dccp_timeout_nla_policy[CTA_TIMEOUT_DCCP_MAX+1] = {
	[CTA_TIMEOUT_DCCP_REQUEST]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_DCCP_RESPOND]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_DCCP_PARTOPEN]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_DCCP_OPEN]		= { .type = NLA_U32 },
	[CTA_TIMEOUT_DCCP_CLOSEREQ]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_DCCP_CLOSING]	= { .type = NLA_U32 },
	[CTA_TIMEOUT_DCCP_TIMEWAIT]	= { .type = NLA_U32 },
};
#endif  

void nf_conntrack_dccp_init_net(struct net *net)
{
	struct nf_dccp_net *dn = nf_dccp_pernet(net);

	 
	dn->dccp_loose = 1;
	dn->dccp_timeout[CT_DCCP_REQUEST]	= 2 * DCCP_MSL;
	dn->dccp_timeout[CT_DCCP_RESPOND]	= 4 * DCCP_MSL;
	dn->dccp_timeout[CT_DCCP_PARTOPEN]	= 4 * DCCP_MSL;
	dn->dccp_timeout[CT_DCCP_OPEN]		= 12 * 3600 * HZ;
	dn->dccp_timeout[CT_DCCP_CLOSEREQ]	= 64 * HZ;
	dn->dccp_timeout[CT_DCCP_CLOSING]	= 64 * HZ;
	dn->dccp_timeout[CT_DCCP_TIMEWAIT]	= 2 * DCCP_MSL;

	 
	dn->dccp_timeout[CT_DCCP_NONE] = dn->dccp_timeout[CT_DCCP_REQUEST];
}

const struct nf_conntrack_l4proto nf_conntrack_l4proto_dccp = {
	.l4proto		= IPPROTO_DCCP,
	.can_early_drop		= dccp_can_early_drop,
#ifdef CONFIG_NF_CONNTRACK_PROCFS
	.print_conntrack	= dccp_print_conntrack,
#endif
#if IS_ENABLED(CONFIG_NF_CT_NETLINK)
	.nlattr_size		= DCCP_NLATTR_SIZE,
	.to_nlattr		= dccp_to_nlattr,
	.from_nlattr		= nlattr_to_dccp,
	.tuple_to_nlattr	= nf_ct_port_tuple_to_nlattr,
	.nlattr_tuple_size	= nf_ct_port_nlattr_tuple_size,
	.nlattr_to_tuple	= nf_ct_port_nlattr_to_tuple,
	.nla_policy		= nf_ct_port_nla_policy,
#endif
#ifdef CONFIG_NF_CONNTRACK_TIMEOUT
	.ctnl_timeout		= {
		.nlattr_to_obj	= dccp_timeout_nlattr_to_obj,
		.obj_to_nlattr	= dccp_timeout_obj_to_nlattr,
		.nlattr_max	= CTA_TIMEOUT_DCCP_MAX,
		.obj_size	= sizeof(unsigned int) * CT_DCCP_MAX,
		.nla_policy	= dccp_timeout_nla_policy,
	},
#endif  
};
