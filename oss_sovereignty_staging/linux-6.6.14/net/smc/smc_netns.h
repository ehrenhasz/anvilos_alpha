 
 

#ifndef SMC_NETNS_H
#define SMC_NETNS_H

#include "smc_pnet.h"

extern unsigned int smc_net_id;

 
struct smc_net {
	struct smc_pnettable pnettable;
	struct smc_pnetids_ndev pnetids_ndev;
};
#endif
