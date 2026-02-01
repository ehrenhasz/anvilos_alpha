
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/netfilter.h>
#include <linux/slab.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/nf_conntrack_irc.h>

#define MAX_PORTS 8
static unsigned short ports[MAX_PORTS];
static unsigned int ports_c;
static unsigned int max_dcc_channels = 8;
static unsigned int dcc_timeout __read_mostly = 300;
 
static char *irc_buffer;
static DEFINE_SPINLOCK(irc_buffer_lock);

unsigned int (*nf_nat_irc_hook)(struct sk_buff *skb,
				enum ip_conntrack_info ctinfo,
				unsigned int protoff,
				unsigned int matchoff,
				unsigned int matchlen,
				struct nf_conntrack_expect *exp) __read_mostly;
EXPORT_SYMBOL_GPL(nf_nat_irc_hook);

#define HELPER_NAME "irc"
#define MAX_SEARCH_SIZE	4095

MODULE_AUTHOR("Harald Welte <laforge@netfilter.org>");
MODULE_DESCRIPTION("IRC (DCC) connection tracking helper");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ip_conntrack_irc");
MODULE_ALIAS_NFCT_HELPER(HELPER_NAME);

module_param_array(ports, ushort, &ports_c, 0400);
MODULE_PARM_DESC(ports, "port numbers of IRC servers");
module_param(max_dcc_channels, uint, 0400);
MODULE_PARM_DESC(max_dcc_channels, "max number of expected DCC channels per "
				   "IRC session");
module_param(dcc_timeout, uint, 0400);
MODULE_PARM_DESC(dcc_timeout, "timeout on for unestablished DCC channels");

static const char *const dccprotos[] = {
	"SEND ", "CHAT ", "MOVE ", "TSEND ", "SCHAT "
};

#define MINMATCHLEN	5

 
static int parse_dcc(char *data, const char *data_end, __be32 *ip,
		     u_int16_t *port, char **ad_beg_p, char **ad_end_p)
{
	char *tmp;

	 
	while (*data++ != ' ')
		if (data > data_end - 12)
			return -1;

	 
	for (tmp = data; tmp <= data_end; tmp++)
		if (*tmp == '\n')
			break;
	if (tmp > data_end || *tmp != '\n')
		return -1;

	*ad_beg_p = data;
	*ip = cpu_to_be32(simple_strtoul(data, &data, 10));

	 
	while (*data == ' ') {
		if (data >= data_end)
			return -1;
		data++;
	}

	*port = simple_strtoul(data, &data, 10);
	*ad_end_p = data;

	return 0;
}

static int help(struct sk_buff *skb, unsigned int protoff,
		struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	unsigned int dataoff;
	const struct iphdr *iph;
	const struct tcphdr *th;
	struct tcphdr _tcph;
	const char *data_limit;
	char *data, *ib_ptr;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_expect *exp;
	struct nf_conntrack_tuple *tuple;
	__be32 dcc_ip;
	u_int16_t dcc_port;
	__be16 port;
	int i, ret = NF_ACCEPT;
	char *addr_beg_p, *addr_end_p;
	typeof(nf_nat_irc_hook) nf_nat_irc;
	unsigned int datalen;

	 
	if (dir == IP_CT_DIR_REPLY)
		return NF_ACCEPT;

	 
	if (ctinfo != IP_CT_ESTABLISHED && ctinfo != IP_CT_ESTABLISHED_REPLY)
		return NF_ACCEPT;

	 
	th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
	if (th == NULL)
		return NF_ACCEPT;

	 
	dataoff = protoff + th->doff*4;
	if (dataoff >= skb->len)
		return NF_ACCEPT;

	datalen = skb->len - dataoff;
	if (datalen > MAX_SEARCH_SIZE)
		datalen = MAX_SEARCH_SIZE;

	spin_lock_bh(&irc_buffer_lock);
	ib_ptr = skb_header_pointer(skb, dataoff, datalen,
				    irc_buffer);
	if (!ib_ptr) {
		spin_unlock_bh(&irc_buffer_lock);
		return NF_ACCEPT;
	}

	data = ib_ptr;
	data_limit = ib_ptr + datalen;

	 
	while (data < data_limit - 10) {
		if (*data == ' ' || *data == '\r' || *data == '\n')
			data++;
		else
			break;
	}

	 
	if (data < data_limit - 10) {
		if (strncasecmp("PRIVMSG ", data, 8))
			goto out;
		data += 8;
	}

	 
	while (data < data_limit - (21 + MINMATCHLEN)) {
		 
		if (memcmp(data, " :", 2)) {
			data++;
			continue;
		}
		data += 2;

		 
		if (memcmp(data, "\1DCC ", 5))
			goto out;
		data += 5;
		 

		iph = ip_hdr(skb);
		pr_debug("DCC found in master %pI4:%u %pI4:%u\n",
			 &iph->saddr, ntohs(th->source),
			 &iph->daddr, ntohs(th->dest));

		for (i = 0; i < ARRAY_SIZE(dccprotos); i++) {
			if (memcmp(data, dccprotos[i], strlen(dccprotos[i]))) {
				 
				continue;
			}
			data += strlen(dccprotos[i]);
			pr_debug("DCC %s detected\n", dccprotos[i]);

			 
			if (parse_dcc(data, data_limit, &dcc_ip,
				       &dcc_port, &addr_beg_p, &addr_end_p)) {
				pr_debug("unable to parse dcc command\n");
				continue;
			}

			pr_debug("DCC bound ip/port: %pI4:%u\n",
				 &dcc_ip, dcc_port);

			 
			tuple = &ct->tuplehash[dir].tuple;
			if ((tuple->src.u3.ip != dcc_ip &&
			     ct->tuplehash[!dir].tuple.dst.u3.ip != dcc_ip) ||
			    dcc_port == 0) {
				net_warn_ratelimited("Forged DCC command from %pI4: %pI4:%u\n",
						     &tuple->src.u3.ip,
						     &dcc_ip, dcc_port);
				continue;
			}

			exp = nf_ct_expect_alloc(ct);
			if (exp == NULL) {
				nf_ct_helper_log(skb, ct,
						 "cannot alloc expectation");
				ret = NF_DROP;
				goto out;
			}
			tuple = &ct->tuplehash[!dir].tuple;
			port = htons(dcc_port);
			nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT,
					  tuple->src.l3num,
					  NULL, &tuple->dst.u3,
					  IPPROTO_TCP, NULL, &port);

			nf_nat_irc = rcu_dereference(nf_nat_irc_hook);
			if (nf_nat_irc && ct->status & IPS_NAT_MASK)
				ret = nf_nat_irc(skb, ctinfo, protoff,
						 addr_beg_p - ib_ptr,
						 addr_end_p - addr_beg_p,
						 exp);
			else if (nf_ct_expect_related(exp, 0) != 0) {
				nf_ct_helper_log(skb, ct,
						 "cannot add expectation");
				ret = NF_DROP;
			}
			nf_ct_expect_put(exp);
			goto out;
		}
	}
 out:
	spin_unlock_bh(&irc_buffer_lock);
	return ret;
}

static struct nf_conntrack_helper irc[MAX_PORTS] __read_mostly;
static struct nf_conntrack_expect_policy irc_exp_policy;

static int __init nf_conntrack_irc_init(void)
{
	int i, ret;

	if (max_dcc_channels < 1) {
		pr_err("max_dcc_channels must not be zero\n");
		return -EINVAL;
	}

	if (max_dcc_channels > NF_CT_EXPECT_MAX_CNT) {
		pr_err("max_dcc_channels must not be more than %u\n",
		       NF_CT_EXPECT_MAX_CNT);
		return -EINVAL;
	}

	irc_exp_policy.max_expected = max_dcc_channels;
	irc_exp_policy.timeout = dcc_timeout;

	irc_buffer = kmalloc(MAX_SEARCH_SIZE + 1, GFP_KERNEL);
	if (!irc_buffer)
		return -ENOMEM;

	 
	if (ports_c == 0)
		ports[ports_c++] = IRC_PORT;

	for (i = 0; i < ports_c; i++) {
		nf_ct_helper_init(&irc[i], AF_INET, IPPROTO_TCP, HELPER_NAME,
				  IRC_PORT, ports[i], i, &irc_exp_policy,
				  0, help, NULL, THIS_MODULE);
	}

	ret = nf_conntrack_helpers_register(&irc[0], ports_c);
	if (ret) {
		pr_err("failed to register helpers\n");
		kfree(irc_buffer);
		return ret;
	}

	return 0;
}

static void __exit nf_conntrack_irc_fini(void)
{
	nf_conntrack_helpers_unregister(irc, ports_c);
	kfree(irc_buffer);
}

module_init(nf_conntrack_irc_init);
module_exit(nf_conntrack_irc_fini);
