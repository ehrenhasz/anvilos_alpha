 
 

#ifndef _MMC_CORE_CRYPTO_H
#define _MMC_CORE_CRYPTO_H

struct mmc_host;
struct mmc_queue_req;
struct request_queue;

#ifdef CONFIG_MMC_CRYPTO

void mmc_crypto_set_initial_state(struct mmc_host *host);

void mmc_crypto_setup_queue(struct request_queue *q, struct mmc_host *host);

void mmc_crypto_prepare_req(struct mmc_queue_req *mqrq);

#else  

static inline void mmc_crypto_set_initial_state(struct mmc_host *host)
{
}

static inline void mmc_crypto_setup_queue(struct request_queue *q,
					  struct mmc_host *host)
{
}

static inline void mmc_crypto_prepare_req(struct mmc_queue_req *mqrq)
{
}

#endif  

#endif  
