
 

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter.h>
#include <linux/slab.h>
#include <linux/in.h>
#include <linux/tcp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <linux/netfilter/nf_conntrack_sane.h>

#define HELPER_NAME "sane"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michal Schmidt <mschmidt@redhat.com>");
MODULE_DESCRIPTION("SANE connection tracking helper");
MODULE_ALIAS_NFCT_HELPER(HELPER_NAME);

#define MAX_PORTS 8
static u_int16_t ports[MAX_PORTS];
static unsigned int ports_c;
module_param_array(ports, ushort, &ports_c, 0400);

struct sane_request {
	__be32 RPC_code;
#define SANE_NET_START      7    

	__be32 handle;
};

struct sane_reply_net_start {
	__be32 status;
#define SANE_STATUS_SUCCESS 0

	__be16 zero;
	__be16 port;
	 
};

static int help(struct sk_buff *skb,
		unsigned int protoff,
		struct nf_conn *ct,
		enum ip_conntrack_info ctinfo)
{
	unsigned int dataoff, datalen;
	const struct tcphdr *th;
	struct tcphdr _tcph;
	int ret = NF_ACCEPT;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_ct_sane_master *ct_sane_info = nfct_help_data(ct);
	struct nf_conntrack_expect *exp;
	struct nf_conntrack_tuple *tuple;
	struct sane_reply_net_start *reply;
	union {
		struct sane_request req;
		struct sane_reply_net_start repl;
	} buf;

	 
	if (ctinfo != IP_CT_ESTABLISHED &&
	    ctinfo != IP_CT_ESTABLISHED_REPLY)
		return NF_ACCEPT;

	 
	th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
	if (th == NULL)
		return NF_ACCEPT;

	 
	dataoff = protoff + th->doff * 4;
	if (dataoff >= skb->len)
		return NF_ACCEPT;

	datalen = skb->len - dataoff;
	if (dir == IP_CT_DIR_ORIGINAL) {
		const struct sane_request *req;

		if (datalen != sizeof(struct sane_request))
			return NF_ACCEPT;

		req = skb_header_pointer(skb, dataoff, datalen, &buf.req);
		if (!req)
			return NF_ACCEPT;

		if (req->RPC_code != htonl(SANE_NET_START)) {
			 
			WRITE_ONCE(ct_sane_info->state, SANE_STATE_NORMAL);
			return NF_ACCEPT;
		}

		 
		WRITE_ONCE(ct_sane_info->state, SANE_STATE_START_REQUESTED);
		return NF_ACCEPT;
	}

	 

	 
	if (READ_ONCE(ct_sane_info->state) != SANE_STATE_START_REQUESTED)
		return NF_ACCEPT;

	 
	WRITE_ONCE(ct_sane_info->state, SANE_STATE_NORMAL);

	if (datalen < sizeof(struct sane_reply_net_start)) {
		pr_debug("NET_START reply too short\n");
		return NF_ACCEPT;
	}

	datalen = sizeof(struct sane_reply_net_start);

	reply = skb_header_pointer(skb, dataoff, datalen, &buf.repl);
	if (!reply)
		return NF_ACCEPT;

	if (reply->status != htonl(SANE_STATUS_SUCCESS)) {
		 
		pr_debug("unsuccessful SANE_STATUS = %u\n",
			 ntohl(reply->status));
		return NF_ACCEPT;
	}

	 
	if (reply->zero != 0)
		return NF_ACCEPT;

	exp = nf_ct_expect_alloc(ct);
	if (exp == NULL) {
		nf_ct_helper_log(skb, ct, "cannot alloc expectation");
		return NF_DROP;
	}

	tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			  &tuple->src.u3, &tuple->dst.u3,
			  IPPROTO_TCP, NULL, &reply->port);

	pr_debug("expect: ");
	nf_ct_dump_tuple(&exp->tuple);

	 
	if (nf_ct_expect_related(exp, 0) != 0) {
		nf_ct_helper_log(skb, ct, "cannot add expectation");
		ret = NF_DROP;
	}

	nf_ct_expect_put(exp);
	return ret;
}

static struct nf_conntrack_helper sane[MAX_PORTS * 2] __read_mostly;

static const struct nf_conntrack_expect_policy sane_exp_policy = {
	.max_expected	= 1,
	.timeout	= 5 * 60,
};

static void __exit nf_conntrack_sane_fini(void)
{
	nf_conntrack_helpers_unregister(sane, ports_c * 2);
}

static int __init nf_conntrack_sane_init(void)
{
	int i, ret = 0;

	NF_CT_HELPER_BUILD_BUG_ON(sizeof(struct nf_ct_sane_master));

	if (ports_c == 0)
		ports[ports_c++] = SANE_PORT;

	 
	for (i = 0; i < ports_c; i++) {
		nf_ct_helper_init(&sane[2 * i], AF_INET, IPPROTO_TCP,
				  HELPER_NAME, SANE_PORT, ports[i], ports[i],
				  &sane_exp_policy, 0, help, NULL,
				  THIS_MODULE);
		nf_ct_helper_init(&sane[2 * i + 1], AF_INET6, IPPROTO_TCP,
				  HELPER_NAME, SANE_PORT, ports[i], ports[i],
				  &sane_exp_policy, 0, help, NULL,
				  THIS_MODULE);
	}

	ret = nf_conntrack_helpers_register(sane, ports_c * 2);
	if (ret < 0) {
		pr_err("failed to register helpers\n");
		return ret;
	}

	return 0;
}

module_init(nf_conntrack_sane_init);
module_exit(nf_conntrack_sane_fini);
