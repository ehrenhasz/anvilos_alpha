


#ifndef __QI_H__
#define __QI_H__

#include <crypto/algapi.h>
#include <linux/compiler_attributes.h>
#include <soc/fsl/qman.h>
#include "compat.h"
#include "desc.h"
#include "desc_constr.h"


#define CAAM_QI_MEMCACHE_SIZE	768

extern bool caam_congested __read_mostly;


struct caam_drv_req;


typedef void (*caam_qi_cbk)(struct caam_drv_req *drv_req, u32 status);

enum optype {
	ENCRYPT,
	DECRYPT,
	NUM_OP
};


struct caam_drv_ctx {
	struct {
		u32 prehdr[2];
		u32 sh_desc[MAX_SDLEN];
	} __aligned(CRYPTO_DMA_ALIGN);
	dma_addr_t context_a;
	struct qman_fq *req_fq;
	struct qman_fq *rsp_fq;
	refcount_t refcnt;
	int cpu;
	enum optype op_type;
	struct device *qidev;
};


struct caam_drv_req {
	struct qm_sg_entry fd_sgt[2];
	struct caam_drv_ctx *drv_ctx;
	caam_qi_cbk cbk;
	void *app_ctx;
} __aligned(CRYPTO_DMA_ALIGN);


struct caam_drv_ctx *caam_drv_ctx_init(struct device *qidev, int *cpu,
				       u32 *sh_desc);


int caam_qi_enqueue(struct device *qidev, struct caam_drv_req *req);


bool caam_drv_ctx_busy(struct caam_drv_ctx *drv_ctx);


int caam_drv_ctx_update(struct caam_drv_ctx *drv_ctx, u32 *sh_desc);


void caam_drv_ctx_rel(struct caam_drv_ctx *drv_ctx);

int caam_qi_init(struct platform_device *pdev);


void *qi_cache_alloc(gfp_t flags);


void qi_cache_free(void *obj);

#endif 
