
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/tcp.h>
#include <linux/kernel.h>

#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <linux/netfilter/nf_conntrack_irc.h>

#define NAT_HELPER_NAME "irc"

MODULE_AUTHOR("Harald Welte <laforge@gnumonks.org>");
MODULE_DESCRIPTION("IRC (DCC) NAT helper");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NF_NAT_HELPER(NAT_HELPER_NAME);

static struct nf_conntrack_nat_helper nat_helper_irc =
	NF_CT_NAT_HELPER_INIT(NAT_HELPER_NAME);

static unsigned int help(struct sk_buff *skb,
			 enum ip_conntrack_info ctinfo,
			 unsigned int protoff,
			 unsigned int matchoff,
			 unsigned int matchlen,
			 struct nf_conntrack_expect *exp)
{
	char buffer[sizeof("4294967296 65635")];
	struct nf_conn *ct = exp->master;
	union nf_inet_addr newaddr;
	u_int16_t port;

	 
	newaddr = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3;

	exp->saved_proto.tcp.port = exp->tuple.dst.u.tcp.port;
	exp->dir = IP_CT_DIR_REPLY;
	exp->expectfn = nf_nat_follow_master;

	port = nf_nat_exp_find_port(exp,
				    ntohs(exp->saved_proto.tcp.port));
	if (port == 0) {
		nf_ct_helper_log(skb, ct, "all ports in use");
		return NF_DROP;
	}

	 
	 
	snprintf(buffer, sizeof(buffer), "%u %u", ntohl(newaddr.ip), port);
	pr_debug("inserting '%s' == %pI4, port %u\n",
		 buffer, &newaddr.ip, port);

	if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo, protoff, matchoff,
				      matchlen, buffer, strlen(buffer))) {
		nf_ct_helper_log(skb, ct, "cannot mangle packet");
		nf_ct_unexpect_related(exp);
		return NF_DROP;
	}

	return NF_ACCEPT;
}

static void __exit nf_nat_irc_fini(void)
{
	nf_nat_helper_unregister(&nat_helper_irc);
	RCU_INIT_POINTER(nf_nat_irc_hook, NULL);
	synchronize_rcu();
}

static int __init nf_nat_irc_init(void)
{
	BUG_ON(nf_nat_irc_hook != NULL);
	nf_nat_helper_register(&nat_helper_irc);
	RCU_INIT_POINTER(nf_nat_irc_hook, help);
	return 0;
}

 
static int warn_set(const char *val, const struct kernel_param *kp)
{
	pr_info("kernel >= 2.6.10 only uses 'ports' for conntrack modules\n");
	return 0;
}
module_param_call(ports, warn_set, NULL, NULL, 0);

module_init(nf_nat_irc_init);
module_exit(nf_nat_irc_fini);
