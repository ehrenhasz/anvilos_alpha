 
 

#ifndef __MMC_HOST_SDHCI_CQHCI_H__
#define __MMC_HOST_SDHCI_CQHCI_H__

#include "cqhci.h"
#include "sdhci.h"

static inline void sdhci_and_cqhci_reset(struct sdhci_host *host, u8 mask)
{
	if ((host->mmc->caps2 & MMC_CAP2_CQE) && (mask & SDHCI_RESET_ALL) &&
	    host->mmc->cqe_private)
		cqhci_deactivate(host->mmc);

	sdhci_reset(host, mask);
}

#endif  
