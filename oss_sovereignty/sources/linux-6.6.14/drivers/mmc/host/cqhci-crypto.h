


#ifndef LINUX_MMC_CQHCI_CRYPTO_H
#define LINUX_MMC_CQHCI_CRYPTO_H

#include <linux/mmc/host.h>

#include "cqhci.h"

#ifdef CONFIG_MMC_CRYPTO

int cqhci_crypto_init(struct cqhci_host *host);


static inline u64 cqhci_crypto_prep_task_desc(struct mmc_request *mrq)
{
	if (!mrq->crypto_ctx)
		return 0;

	
	WARN_ON_ONCE(mrq->crypto_ctx->bc_dun[0] > U32_MAX);

	return CQHCI_CRYPTO_ENABLE_BIT |
	       CQHCI_CRYPTO_KEYSLOT(mrq->crypto_key_slot) |
	       mrq->crypto_ctx->bc_dun[0];
}

#else 

static inline int cqhci_crypto_init(struct cqhci_host *host)
{
	return 0;
}

static inline u64 cqhci_crypto_prep_task_desc(struct mmc_request *mrq)
{
	return 0;
}

#endif 

#endif 
