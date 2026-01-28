#include <linux/dma-mapping.h>
#ifndef _CRYPTO_OCS_HCU_H
#define _CRYPTO_OCS_HCU_H
#define OCS_HCU_DMA_BIT_MASK		DMA_BIT_MASK(32)
#define OCS_HCU_HW_KEY_LEN		64
struct ocs_hcu_dma_list;
enum ocs_hcu_algo {
	OCS_HCU_ALGO_SHA256 = 2,
	OCS_HCU_ALGO_SHA224 = 3,
	OCS_HCU_ALGO_SHA384 = 4,
	OCS_HCU_ALGO_SHA512 = 5,
	OCS_HCU_ALGO_SM3    = 6,
};
struct ocs_hcu_dev {
	struct list_head list;
	struct device *dev;
	void __iomem *io_base;
	struct crypto_engine *engine;
	int irq;
	struct completion irq_done;
	bool irq_err;
};
struct ocs_hcu_idata {
	u32 msg_len_lo;
	u32 msg_len_hi;
	u8  digest[SHA512_DIGEST_SIZE];
};
struct ocs_hcu_hash_ctx {
	enum ocs_hcu_algo	algo;
	struct ocs_hcu_idata	idata;
};
irqreturn_t ocs_hcu_irq_handler(int irq, void *dev_id);
struct ocs_hcu_dma_list *ocs_hcu_dma_list_alloc(struct ocs_hcu_dev *hcu_dev,
						int max_nents);
void ocs_hcu_dma_list_free(struct ocs_hcu_dev *hcu_dev,
			   struct ocs_hcu_dma_list *dma_list);
int ocs_hcu_dma_list_add_tail(struct ocs_hcu_dev *hcu_dev,
			      struct ocs_hcu_dma_list *dma_list,
			      dma_addr_t addr, u32 len);
int ocs_hcu_hash_init(struct ocs_hcu_hash_ctx *ctx, enum ocs_hcu_algo algo);
int ocs_hcu_hash_update(struct ocs_hcu_dev *hcu_dev,
			struct ocs_hcu_hash_ctx *ctx,
			const struct ocs_hcu_dma_list *dma_list);
int ocs_hcu_hash_finup(struct ocs_hcu_dev *hcu_dev,
		       const struct ocs_hcu_hash_ctx *ctx,
		       const struct ocs_hcu_dma_list *dma_list,
		       u8 *dgst, size_t dgst_len);
int ocs_hcu_hash_final(struct ocs_hcu_dev *hcu_dev,
		       const struct ocs_hcu_hash_ctx *ctx, u8 *dgst,
		       size_t dgst_len);
int ocs_hcu_digest(struct ocs_hcu_dev *hcu_dev, enum ocs_hcu_algo algo,
		   void *data, size_t data_len, u8 *dgst, size_t dgst_len);
int ocs_hcu_hmac(struct ocs_hcu_dev *hcu_dev, enum ocs_hcu_algo algo,
		 const u8 *key, size_t key_len,
		 const struct ocs_hcu_dma_list *dma_list,
		 u8 *dgst, size_t dgst_len);
#endif  
