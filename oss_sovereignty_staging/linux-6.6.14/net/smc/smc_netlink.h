 
 

#ifndef _SMC_NETLINK_H
#define _SMC_NETLINK_H

#include <net/netlink.h>
#include <net/genetlink.h>

extern struct genl_family smc_gen_nl_family;

extern const struct nla_policy smc_gen_ueid_policy[];

struct smc_nl_dmp_ctx {
	int pos[3];
};

static inline struct smc_nl_dmp_ctx *smc_nl_dmp_ctx(struct netlink_callback *c)
{
	return (struct smc_nl_dmp_ctx *)c->ctx;
}

int smc_nl_init(void) __init;
void smc_nl_exit(void);

#endif
