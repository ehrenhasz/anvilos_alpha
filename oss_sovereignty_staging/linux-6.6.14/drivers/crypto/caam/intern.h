 
 

#ifndef INTERN_H
#define INTERN_H

#include "ctrl.h"
#include <crypto/engine.h>

 
#define JOBR_DEPTH (1 << CONFIG_CRYPTO_DEV_FSL_CAAM_RINGSIZE)

 
#define THRESHOLD 15
#define CRYPTO_ENGINE_MAX_QLEN (JOBR_DEPTH - THRESHOLD)

 
#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_INTC
#define JOBR_INTC JRCFG_ICEN
#define JOBR_INTC_TIME_THLD CONFIG_CRYPTO_DEV_FSL_CAAM_INTC_TIME_THLD
#define JOBR_INTC_COUNT_THLD CONFIG_CRYPTO_DEV_FSL_CAAM_INTC_COUNT_THLD
#else
#define JOBR_INTC 0
#define JOBR_INTC_TIME_THLD 0
#define JOBR_INTC_COUNT_THLD 0
#endif

 
struct caam_jrentry_info {
	void (*callbk)(struct device *dev, u32 *desc, u32 status, void *arg);
	void *cbkarg;	 
	u32 *desc_addr_virt;	 
	dma_addr_t desc_addr_dma;	 
	u32 desc_size;	 
};

struct caam_jr_state {
	dma_addr_t inpbusaddr;
	dma_addr_t outbusaddr;
};

struct caam_jr_dequeue_params {
	struct device *dev;
	int enable_itr;
};

 
struct caam_drv_private_jr {
	struct list_head	list_node;	 
	struct device		*dev;
	int ridx;
	struct caam_job_ring __iomem *rregs;	 
	struct tasklet_struct irqtask;
	struct caam_jr_dequeue_params tasklet_params;
	int irq;			 
	bool hwrng;

	 
	atomic_t tfm_count ____cacheline_aligned;

	 
	struct caam_jrentry_info *entinfo;	 
	spinlock_t inplock ____cacheline_aligned;  
	u32 inpring_avail;	 
	int head;			 
	void *inpring;			 
	int out_ring_read_index;	 
	int tail;			 
	void *outring;			 
	struct crypto_engine *engine;

	struct caam_jr_state state;	 
};

struct caam_ctl_state {
	struct masterid deco_mid[16];
	struct masterid jr_mid[4];
	u32 mcr;
	u32 scfgr;
};

 
struct caam_drv_private {
	 
	struct caam_ctrl __iomem *ctrl;  
	struct caam_deco __iomem *deco;  
	struct caam_assurance __iomem *assure;
	struct caam_queue_if __iomem *qi;  
	struct caam_job_ring __iomem *jr[4];	 

	struct iommu_domain *domain;

	 
	u8 total_jobrs;		 
	u8 qi_present;		 
	u8 blob_present;	 
	u8 mc_en;		 
	u8 optee_en;		 
	bool pr_support;         
	int secvio_irq;		 
	int virt_en;		 
	int era;		 

#define	RNG4_MAX_HANDLES 2
	 
	u32 rng4_sh_init;	 

	struct clk_bulk_data *clks;
	int num_clks;
	 
#ifdef CONFIG_DEBUG_FS
	struct dentry *ctl;  
	struct debugfs_blob_wrapper ctl_kek_wrap, ctl_tkek_wrap, ctl_tdsk_wrap;
#endif

	int caam_off_during_pm;		 
	struct caam_ctl_state state;	 
};

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_CRYPTO_API

int caam_algapi_init(struct device *dev);
void caam_algapi_exit(void);

#else

static inline int caam_algapi_init(struct device *dev)
{
	return 0;
}

static inline void caam_algapi_exit(void)
{
}

#endif  

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_AHASH_API

int caam_algapi_hash_init(struct device *dev);
void caam_algapi_hash_exit(void);

#else

static inline int caam_algapi_hash_init(struct device *dev)
{
	return 0;
}

static inline void caam_algapi_hash_exit(void)
{
}

#endif  

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_PKC_API

int caam_pkc_init(struct device *dev);
void caam_pkc_exit(void);

#else

static inline int caam_pkc_init(struct device *dev)
{
	return 0;
}

static inline void caam_pkc_exit(void)
{
}

#endif  

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_RNG_API

int caam_rng_init(struct device *dev);
void caam_rng_exit(struct device *dev);

#else

static inline int caam_rng_init(struct device *dev)
{
	return 0;
}

static inline void caam_rng_exit(struct device *dev) {}

#endif  

#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_PRNG_API

int caam_prng_register(struct device *dev);
void caam_prng_unregister(void *data);

#else

static inline int caam_prng_register(struct device *dev)
{
	return 0;
}

static inline void caam_prng_unregister(void *data) {}
#endif  

#ifdef CONFIG_CAAM_QI

int caam_qi_algapi_init(struct device *dev);
void caam_qi_algapi_exit(void);

#else

static inline int caam_qi_algapi_init(struct device *dev)
{
	return 0;
}

static inline void caam_qi_algapi_exit(void)
{
}

#endif  

static inline u64 caam_get_dma_mask(struct device *dev)
{
	struct device_node *nprop = dev->of_node;

	if (caam_ptr_sz != sizeof(u64))
		return DMA_BIT_MASK(32);

	if (caam_dpaa2)
		return DMA_BIT_MASK(49);

	if (of_device_is_compatible(nprop, "fsl,sec-v5.0-job-ring") ||
	    of_device_is_compatible(nprop, "fsl,sec-v5.0"))
		return DMA_BIT_MASK(40);

	return DMA_BIT_MASK(36);
}


#endif  
