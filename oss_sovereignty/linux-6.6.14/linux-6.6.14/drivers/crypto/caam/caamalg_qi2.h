#ifndef _CAAMALG_QI2_H_
#define _CAAMALG_QI2_H_
#include <crypto/internal/skcipher.h>
#include <linux/compiler_attributes.h>
#include <soc/fsl/dpaa2-io.h>
#include <soc/fsl/dpaa2-fd.h>
#include <linux/threads.h>
#include <linux/netdevice.h>
#include "dpseci.h"
#include "desc_constr.h"
#define DPAA2_CAAM_STORE_SIZE	16
#define DPAA2_CAAM_NAPI_WEIGHT	512
#define DPAA2_SEC_CONG_ENTRY_THRESH	(128 * 1024 * 1024)
#define DPAA2_SEC_CONG_EXIT_THRESH	(DPAA2_SEC_CONG_ENTRY_THRESH * 9 / 10)
struct dpaa2_caam_priv {
	int dpsec_id;
	u16 major_ver;
	u16 minor_ver;
	struct dpseci_attr dpseci_attr;
	struct dpseci_sec_attr sec_attr;
	struct dpseci_rx_queue_attr rx_queue_attr[DPSECI_MAX_QUEUE_NUM];
	struct dpseci_tx_queue_attr tx_queue_attr[DPSECI_MAX_QUEUE_NUM];
	int num_pairs;
	void *cscn_mem;
	dma_addr_t cscn_dma;
	struct device *dev;
	struct fsl_mc_io *mc_io;
	struct iommu_domain *domain;
	struct dpaa2_caam_priv_per_cpu __percpu *ppriv;
	struct dentry *dfs_root;
};
struct dpaa2_caam_priv_per_cpu {
	struct napi_struct napi;
	struct net_device net_dev;
	int req_fqid;
	int rsp_fqid;
	int prio;
	struct dpaa2_io_notification_ctx nctx;
	struct dpaa2_io_store *store;
	struct dpaa2_caam_priv *priv;
	struct dpaa2_io *dpio;
};
#define CAAM_QI_MEMCACHE_SIZE	512
struct aead_edesc {
	int src_nents;
	int dst_nents;
	dma_addr_t iv_dma;
	int qm_sg_bytes;
	dma_addr_t qm_sg_dma;
	unsigned int assoclen;
	dma_addr_t assoclen_dma;
	struct dpaa2_sg_entry sgt[];
};
struct skcipher_edesc {
	int src_nents;
	int dst_nents;
	dma_addr_t iv_dma;
	int qm_sg_bytes;
	dma_addr_t qm_sg_dma;
	struct dpaa2_sg_entry sgt[];
};
struct ahash_edesc {
	dma_addr_t qm_sg_dma;
	int src_nents;
	int qm_sg_bytes;
	struct dpaa2_sg_entry sgt[];
};
struct caam_flc {
	u32 flc[16];
	u32 sh_desc[MAX_SDLEN];
} __aligned(CRYPTO_DMA_ALIGN);
enum optype {
	ENCRYPT = 0,
	DECRYPT,
	NUM_OP
};
struct caam_request {
	struct dpaa2_fl_entry fd_flt[2] __aligned(CRYPTO_DMA_ALIGN);
	dma_addr_t fd_flt_dma;
	struct caam_flc *flc;
	dma_addr_t flc_dma;
	void (*cbk)(void *ctx, u32 err);
	void *ctx;
	void *edesc;
	struct skcipher_request fallback_req;
};
int dpaa2_caam_enqueue(struct device *dev, struct caam_request *req);
#endif	 
