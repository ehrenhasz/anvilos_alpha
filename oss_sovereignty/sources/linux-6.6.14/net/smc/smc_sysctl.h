


#ifndef _SMC_SYSCTL_H
#define _SMC_SYSCTL_H

#ifdef CONFIG_SYSCTL

int __net_init smc_sysctl_net_init(struct net *net);
void __net_exit smc_sysctl_net_exit(struct net *net);

#else

static inline int smc_sysctl_net_init(struct net *net)
{
	net->smc.sysctl_autocorking_size = SMC_AUTOCORKING_DEFAULT_SIZE;
	return 0;
}

static inline void smc_sysctl_net_exit(struct net *net) { }

#endif 

#endif 
