
 

#include <crypto/engine.h>
#include <crypto/hmac.h>
#include <crypto/internal/hash.h>
#include <crypto/scatterwalk.h>
#include <crypto/sha2.h>
#include <crypto/sm3.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>

#include "ocs-hcu.h"

#define DRV_NAME	"keembay-ocs-hcu"

 
#define REQ_FINAL			BIT(0)
 
#define REQ_FLAGS_HMAC			BIT(1)
 
#define REQ_FLAGS_HMAC_HW		BIT(2)
 
#define REQ_FLAGS_HMAC_SW		BIT(3)

 
struct ocs_hcu_ctx {
	struct ocs_hcu_dev *hcu_dev;
	u8 key[SHA512_BLOCK_SIZE];
	size_t key_len;
	bool is_sm3_tfm;
	bool is_hmac_tfm;
};

 
struct ocs_hcu_rctx {
	struct ocs_hcu_dev	*hcu_dev;
	u32			flags;
	enum ocs_hcu_algo	algo;
	size_t			blk_sz;
	size_t			dig_sz;
	struct ocs_hcu_dma_list	*dma_list;
	struct ocs_hcu_hash_ctx	hash_ctx;
	 
	u8			buffer[2 * SHA512_BLOCK_SIZE];
	size_t			buf_cnt;
	dma_addr_t		buf_dma_addr;
	size_t			buf_dma_count;
	struct scatterlist	*sg;
	unsigned int		sg_data_total;
	unsigned int		sg_data_offset;
	unsigned int		sg_dma_nents;
};

 
struct ocs_hcu_drv {
	struct list_head dev_list;
	spinlock_t lock;  
};

static struct ocs_hcu_drv ocs_hcu = {
	.dev_list = LIST_HEAD_INIT(ocs_hcu.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(ocs_hcu.lock),
};

 
static inline unsigned int kmb_get_total_data(struct ocs_hcu_rctx *rctx)
{
	return rctx->sg_data_total + rctx->buf_cnt;
}

 
static int flush_sg_to_ocs_buffer(struct ocs_hcu_rctx *rctx)
{
	size_t count;

	if (rctx->sg_data_total > (sizeof(rctx->buffer) - rctx->buf_cnt)) {
		WARN(1, "%s: sg data does not fit in buffer\n", __func__);
		return -EINVAL;
	}

	while (rctx->sg_data_total) {
		if (!rctx->sg) {
			WARN(1, "%s: unexpected NULL sg\n", __func__);
			return -EINVAL;
		}
		 
		if (rctx->sg_data_offset == rctx->sg->length) {
			rctx->sg = sg_next(rctx->sg);
			rctx->sg_data_offset = 0;
			continue;
		}
		 
		count = min(rctx->sg->length - rctx->sg_data_offset,
			    rctx->sg_data_total);
		 
		scatterwalk_map_and_copy(&rctx->buffer[rctx->buf_cnt],
					 rctx->sg, rctx->sg_data_offset,
					 count, 0);

		rctx->sg_data_offset += count;
		rctx->sg_data_total -= count;
		rctx->buf_cnt += count;
	}

	return 0;
}

static struct ocs_hcu_dev *kmb_ocs_hcu_find_dev(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ocs_hcu_ctx *tctx = crypto_ahash_ctx(tfm);

	 
	if (tctx->hcu_dev)
		return tctx->hcu_dev;

	 
	spin_lock_bh(&ocs_hcu.lock);
	tctx->hcu_dev = list_first_entry_or_null(&ocs_hcu.dev_list,
						 struct ocs_hcu_dev,
						 list);
	spin_unlock_bh(&ocs_hcu.lock);

	return tctx->hcu_dev;
}

 
static void kmb_ocs_hcu_dma_cleanup(struct ahash_request *req,
				    struct ocs_hcu_rctx *rctx)
{
	struct ocs_hcu_dev *hcu_dev = rctx->hcu_dev;
	struct device *dev = hcu_dev->dev;

	 
	if (rctx->buf_dma_count) {
		dma_unmap_single(dev, rctx->buf_dma_addr, rctx->buf_dma_count,
				 DMA_TO_DEVICE);
		rctx->buf_dma_count = 0;
	}

	 
	if (rctx->sg_dma_nents) {
		dma_unmap_sg(dev, req->src, rctx->sg_dma_nents, DMA_TO_DEVICE);
		rctx->sg_dma_nents = 0;
	}

	 
	if (rctx->dma_list) {
		ocs_hcu_dma_list_free(hcu_dev, rctx->dma_list);
		rctx->dma_list = NULL;
	}
}

 
static int kmb_ocs_dma_prepare(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);
	struct device *dev = rctx->hcu_dev->dev;
	unsigned int remainder = 0;
	unsigned int total;
	size_t nents;
	size_t count;
	int rc;
	int i;

	 
	total = kmb_get_total_data(rctx);
	if (!total)
		return -EINVAL;

	 
	if (!(rctx->flags & REQ_FINAL))
		remainder = total % rctx->blk_sz;

	 
	nents = sg_nents_for_len(req->src, rctx->sg_data_total - remainder);

	 
	if (nents) {
		rctx->sg_dma_nents = dma_map_sg(dev, req->src, nents,
						DMA_TO_DEVICE);
		if (!rctx->sg_dma_nents) {
			dev_err(dev, "Failed to MAP SG\n");
			rc = -ENOMEM;
			goto cleanup;
		}
		 
		nents = rctx->sg_dma_nents;
	}

	 
	if (rctx->buf_cnt) {
		rctx->buf_dma_addr = dma_map_single(dev, rctx->buffer,
						    rctx->buf_cnt,
						    DMA_TO_DEVICE);
		if (dma_mapping_error(dev, rctx->buf_dma_addr)) {
			dev_err(dev, "Failed to map request context buffer\n");
			rc = -ENOMEM;
			goto cleanup;
		}
		rctx->buf_dma_count = rctx->buf_cnt;
		 
		nents++;
	}

	 
	rctx->dma_list = ocs_hcu_dma_list_alloc(rctx->hcu_dev, nents);
	if (!rctx->dma_list) {
		rc = -ENOMEM;
		goto cleanup;
	}

	 
	if (rctx->buf_dma_count) {
		rc = ocs_hcu_dma_list_add_tail(rctx->hcu_dev, rctx->dma_list,
					       rctx->buf_dma_addr,
					       rctx->buf_dma_count);
		if (rc)
			goto cleanup;
	}

	 
	for_each_sg(req->src, rctx->sg, rctx->sg_dma_nents, i) {
		 
		count = min(rctx->sg_data_total - remainder,
			    sg_dma_len(rctx->sg) - rctx->sg_data_offset);
		 
		if (count == 0)
			continue;
		 
		rc = ocs_hcu_dma_list_add_tail(rctx->hcu_dev,
					       rctx->dma_list,
					       rctx->sg->dma_address,
					       count);
		if (rc)
			goto cleanup;

		 
		rctx->sg_data_total -= count;

		 
		if (rctx->sg_data_total <= remainder) {
			WARN_ON(rctx->sg_data_total < remainder);
			rctx->sg_data_offset += count;
			break;
		}

		 
		rctx->sg_data_offset = 0;
	}

	return 0;
cleanup:
	dev_err(dev, "Failed to prepare DMA.\n");
	kmb_ocs_hcu_dma_cleanup(req, rctx);

	return rc;
}

static void kmb_ocs_hcu_secure_cleanup(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);

	 
	memzero_explicit(rctx->buffer, sizeof(rctx->buffer));
}

static int kmb_ocs_hcu_handle_queue(struct ahash_request *req)
{
	struct ocs_hcu_dev *hcu_dev = kmb_ocs_hcu_find_dev(req);

	if (!hcu_dev)
		return -ENOENT;

	return crypto_transfer_hash_request_to_engine(hcu_dev->engine, req);
}

static int prepare_ipad(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ocs_hcu_ctx *ctx = crypto_ahash_ctx(tfm);
	int i;

	WARN(rctx->buf_cnt, "%s: Context buffer is not empty\n", __func__);
	WARN(!(rctx->flags & REQ_FLAGS_HMAC_SW),
	     "%s: HMAC_SW flag is not set\n", __func__);
	 
	if (ctx->key_len > rctx->blk_sz) {
		WARN(1, "%s: Invalid key length in tfm context\n", __func__);
		return -EINVAL;
	}
	memzero_explicit(&ctx->key[ctx->key_len],
			 rctx->blk_sz - ctx->key_len);
	ctx->key_len = rctx->blk_sz;
	 
	for (i = 0; i < rctx->blk_sz; i++)
		rctx->buffer[i] = ctx->key[i] ^ HMAC_IPAD_VALUE;
	rctx->buf_cnt = rctx->blk_sz;

	return 0;
}

static int kmb_ocs_hcu_do_one_request(struct crypto_engine *engine, void *areq)
{
	struct ahash_request *req = container_of(areq, struct ahash_request,
						 base);
	struct ocs_hcu_dev *hcu_dev = kmb_ocs_hcu_find_dev(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);
	struct ocs_hcu_ctx *tctx = crypto_ahash_ctx(tfm);
	int rc;
	int i;

	if (!hcu_dev) {
		rc = -ENOENT;
		goto error;
	}

	 
	if (rctx->flags & REQ_FLAGS_HMAC_HW) {
		 
		rc = kmb_ocs_dma_prepare(req);
		if (rc)
			goto error;

		rc = ocs_hcu_hmac(hcu_dev, rctx->algo, tctx->key, tctx->key_len,
				  rctx->dma_list, req->result, rctx->dig_sz);

		 
		kmb_ocs_hcu_dma_cleanup(req, rctx);

		 
		if (rc)
			goto error;

		goto done;
	}

	 
	if (!(rctx->flags & REQ_FINAL)) {
		 
		if (!kmb_get_total_data(rctx))
			return -EINVAL;

		 
		rc = kmb_ocs_dma_prepare(req);
		if (rc)
			goto error;

		 
		rc = ocs_hcu_hash_update(hcu_dev, &rctx->hash_ctx,
					 rctx->dma_list);

		 
		kmb_ocs_hcu_dma_cleanup(req, rctx);

		 
		if (rc)
			goto error;

		 
		rctx->buf_cnt = 0;
		 
		rc = flush_sg_to_ocs_buffer(rctx);
		if (rc)
			goto error;

		goto done;
	}

	 

	 
	if (kmb_get_total_data(rctx)) {
		 
		rc = kmb_ocs_dma_prepare(req);
		if (rc)
			goto error;

		 
		rc = ocs_hcu_hash_finup(hcu_dev, &rctx->hash_ctx,
					rctx->dma_list,
					req->result, rctx->dig_sz);
		 
		kmb_ocs_hcu_dma_cleanup(req, rctx);

		 
		if (rc)
			goto error;

	} else {   
		rc = ocs_hcu_hash_final(hcu_dev, &rctx->hash_ctx, req->result,
					rctx->dig_sz);
		if (rc)
			goto error;
	}

	 
	if (rctx->flags & REQ_FLAGS_HMAC_SW) {
		 
		WARN_ON(tctx->key_len != rctx->blk_sz);
		for (i = 0; i < rctx->blk_sz; i++)
			rctx->buffer[i] = tctx->key[i] ^ HMAC_OPAD_VALUE;
		 
		for (i = 0; (i < rctx->dig_sz); i++)
			rctx->buffer[rctx->blk_sz + i] = req->result[i];

		 
		rc = ocs_hcu_digest(hcu_dev, rctx->algo, rctx->buffer,
				    rctx->blk_sz + rctx->dig_sz, req->result,
				    rctx->dig_sz);
		if (rc)
			goto error;
	}

	 
	kmb_ocs_hcu_secure_cleanup(req);
done:
	crypto_finalize_hash_request(hcu_dev->engine, req, 0);

	return 0;

error:
	kmb_ocs_hcu_secure_cleanup(req);
	return rc;
}

static int kmb_ocs_hcu_init(struct ahash_request *req)
{
	struct ocs_hcu_dev *hcu_dev = kmb_ocs_hcu_find_dev(req);
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ocs_hcu_ctx *ctx = crypto_ahash_ctx(tfm);

	if (!hcu_dev)
		return -ENOENT;

	 
	memset(rctx, 0, sizeof(*rctx));

	rctx->hcu_dev = hcu_dev;
	rctx->dig_sz = crypto_ahash_digestsize(tfm);

	switch (rctx->dig_sz) {
#ifdef CONFIG_CRYPTO_DEV_KEEMBAY_OCS_HCU_HMAC_SHA224
	case SHA224_DIGEST_SIZE:
		rctx->blk_sz = SHA224_BLOCK_SIZE;
		rctx->algo = OCS_HCU_ALGO_SHA224;
		break;
#endif  
	case SHA256_DIGEST_SIZE:
		rctx->blk_sz = SHA256_BLOCK_SIZE;
		 
		rctx->algo = ctx->is_sm3_tfm ? OCS_HCU_ALGO_SM3 :
					       OCS_HCU_ALGO_SHA256;
		break;
	case SHA384_DIGEST_SIZE:
		rctx->blk_sz = SHA384_BLOCK_SIZE;
		rctx->algo = OCS_HCU_ALGO_SHA384;
		break;
	case SHA512_DIGEST_SIZE:
		rctx->blk_sz = SHA512_BLOCK_SIZE;
		rctx->algo = OCS_HCU_ALGO_SHA512;
		break;
	default:
		return -EINVAL;
	}

	 
	ocs_hcu_hash_init(&rctx->hash_ctx, rctx->algo);

	 
	if (ctx->is_hmac_tfm)
		rctx->flags |= REQ_FLAGS_HMAC;

	return 0;
}

static int kmb_ocs_hcu_update(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);
	int rc;

	if (!req->nbytes)
		return 0;

	rctx->sg_data_total = req->nbytes;
	rctx->sg_data_offset = 0;
	rctx->sg = req->src;

	 
	if (rctx->flags & REQ_FLAGS_HMAC &&
	    !(rctx->flags & REQ_FLAGS_HMAC_SW)) {
		rctx->flags |= REQ_FLAGS_HMAC_SW;
		rc = prepare_ipad(req);
		if (rc)
			return rc;
	}

	 
	if (rctx->sg_data_total <= (sizeof(rctx->buffer) - rctx->buf_cnt))
		return flush_sg_to_ocs_buffer(rctx);

	return kmb_ocs_hcu_handle_queue(req);
}

 
static int kmb_ocs_hcu_fin_common(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct ocs_hcu_ctx *ctx = crypto_ahash_ctx(tfm);
	int rc;

	rctx->flags |= REQ_FINAL;

	 
	if (rctx->flags & REQ_FLAGS_HMAC &&
	    !(rctx->flags & REQ_FLAGS_HMAC_SW)) {
		 
		if (kmb_get_total_data(rctx) &&
		    ctx->key_len <= OCS_HCU_HW_KEY_LEN) {
			rctx->flags |= REQ_FLAGS_HMAC_HW;
		} else {
			rctx->flags |= REQ_FLAGS_HMAC_SW;
			rc = prepare_ipad(req);
			if (rc)
				return rc;
		}
	}

	return kmb_ocs_hcu_handle_queue(req);
}

static int kmb_ocs_hcu_final(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);

	rctx->sg_data_total = 0;
	rctx->sg_data_offset = 0;
	rctx->sg = NULL;

	return kmb_ocs_hcu_fin_common(req);
}

static int kmb_ocs_hcu_finup(struct ahash_request *req)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);

	rctx->sg_data_total = req->nbytes;
	rctx->sg_data_offset = 0;
	rctx->sg = req->src;

	return kmb_ocs_hcu_fin_common(req);
}

static int kmb_ocs_hcu_digest(struct ahash_request *req)
{
	int rc = 0;
	struct ocs_hcu_dev *hcu_dev = kmb_ocs_hcu_find_dev(req);

	if (!hcu_dev)
		return -ENOENT;

	rc = kmb_ocs_hcu_init(req);
	if (rc)
		return rc;

	rc = kmb_ocs_hcu_finup(req);

	return rc;
}

static int kmb_ocs_hcu_export(struct ahash_request *req, void *out)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);

	 
	memcpy(out, rctx, sizeof(*rctx));

	return 0;
}

static int kmb_ocs_hcu_import(struct ahash_request *req, const void *in)
{
	struct ocs_hcu_rctx *rctx = ahash_request_ctx_dma(req);

	 
	memcpy(rctx, in, sizeof(*rctx));

	return 0;
}

static int kmb_ocs_hcu_setkey(struct crypto_ahash *tfm, const u8 *key,
			      unsigned int keylen)
{
	unsigned int digestsize = crypto_ahash_digestsize(tfm);
	struct ocs_hcu_ctx *ctx = crypto_ahash_ctx(tfm);
	size_t blk_sz = crypto_ahash_blocksize(tfm);
	struct crypto_ahash *ahash_tfm;
	struct ahash_request *req;
	struct crypto_wait wait;
	struct scatterlist sg;
	const char *alg_name;
	int rc;

	 
	if (keylen <= blk_sz) {
		memcpy(ctx->key, key, keylen);
		ctx->key_len = keylen;
		return 0;
	}

	switch (digestsize) {
#ifdef CONFIG_CRYPTO_DEV_KEEMBAY_OCS_HCU_HMAC_SHA224
	case SHA224_DIGEST_SIZE:
		alg_name = "sha224-keembay-ocs";
		break;
#endif  
	case SHA256_DIGEST_SIZE:
		alg_name = ctx->is_sm3_tfm ? "sm3-keembay-ocs" :
					     "sha256-keembay-ocs";
		break;
	case SHA384_DIGEST_SIZE:
		alg_name = "sha384-keembay-ocs";
		break;
	case SHA512_DIGEST_SIZE:
		alg_name = "sha512-keembay-ocs";
		break;
	default:
		return -EINVAL;
	}

	ahash_tfm = crypto_alloc_ahash(alg_name, 0, 0);
	if (IS_ERR(ahash_tfm))
		return PTR_ERR(ahash_tfm);

	req = ahash_request_alloc(ahash_tfm, GFP_KERNEL);
	if (!req) {
		rc = -ENOMEM;
		goto err_free_ahash;
	}

	crypto_init_wait(&wait);
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   crypto_req_done, &wait);
	crypto_ahash_clear_flags(ahash_tfm, ~0);

	sg_init_one(&sg, key, keylen);
	ahash_request_set_crypt(req, &sg, ctx->key, keylen);

	rc = crypto_wait_req(crypto_ahash_digest(req), &wait);
	if (rc == 0)
		ctx->key_len = digestsize;

	ahash_request_free(req);
err_free_ahash:
	crypto_free_ahash(ahash_tfm);

	return rc;
}

 
static void __cra_init(struct crypto_tfm *tfm, struct ocs_hcu_ctx *ctx)
{
	crypto_ahash_set_reqsize_dma(__crypto_ahash_cast(tfm),
				     sizeof(struct ocs_hcu_rctx));
}

static int kmb_ocs_hcu_sha_cra_init(struct crypto_tfm *tfm)
{
	struct ocs_hcu_ctx *ctx = crypto_tfm_ctx(tfm);

	__cra_init(tfm, ctx);

	return 0;
}

static int kmb_ocs_hcu_sm3_cra_init(struct crypto_tfm *tfm)
{
	struct ocs_hcu_ctx *ctx = crypto_tfm_ctx(tfm);

	__cra_init(tfm, ctx);

	ctx->is_sm3_tfm = true;

	return 0;
}

static int kmb_ocs_hcu_hmac_sm3_cra_init(struct crypto_tfm *tfm)
{
	struct ocs_hcu_ctx *ctx = crypto_tfm_ctx(tfm);

	__cra_init(tfm, ctx);

	ctx->is_sm3_tfm = true;
	ctx->is_hmac_tfm = true;

	return 0;
}

static int kmb_ocs_hcu_hmac_cra_init(struct crypto_tfm *tfm)
{
	struct ocs_hcu_ctx *ctx = crypto_tfm_ctx(tfm);

	__cra_init(tfm, ctx);

	ctx->is_hmac_tfm = true;

	return 0;
}

 
static void kmb_ocs_hcu_hmac_cra_exit(struct crypto_tfm *tfm)
{
	struct ocs_hcu_ctx *ctx = crypto_tfm_ctx(tfm);

	 
	memzero_explicit(ctx->key, sizeof(ctx->key));
}

static struct ahash_engine_alg ocs_hcu_algs[] = {
#ifdef CONFIG_CRYPTO_DEV_KEEMBAY_OCS_HCU_HMAC_SHA224
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.halg = {
		.digestsize	= SHA224_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "sha224",
			.cra_driver_name	= "sha224-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA224_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_sha_cra_init,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.setkey		= kmb_ocs_hcu_setkey,
	.base.halg = {
		.digestsize	= SHA224_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "hmac(sha224)",
			.cra_driver_name	= "hmac-sha224-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA224_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_hmac_cra_init,
			.cra_exit		= kmb_ocs_hcu_hmac_cra_exit,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
#endif  
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.halg = {
		.digestsize	= SHA256_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "sha256",
			.cra_driver_name	= "sha256-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA256_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_sha_cra_init,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.setkey		= kmb_ocs_hcu_setkey,
	.base.halg = {
		.digestsize	= SHA256_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "hmac(sha256)",
			.cra_driver_name	= "hmac-sha256-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA256_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_hmac_cra_init,
			.cra_exit		= kmb_ocs_hcu_hmac_cra_exit,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.halg = {
		.digestsize	= SM3_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "sm3",
			.cra_driver_name	= "sm3-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SM3_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_sm3_cra_init,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.setkey		= kmb_ocs_hcu_setkey,
	.base.halg = {
		.digestsize	= SM3_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "hmac(sm3)",
			.cra_driver_name	= "hmac-sm3-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SM3_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_hmac_sm3_cra_init,
			.cra_exit		= kmb_ocs_hcu_hmac_cra_exit,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.halg = {
		.digestsize	= SHA384_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "sha384",
			.cra_driver_name	= "sha384-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA384_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_sha_cra_init,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.setkey		= kmb_ocs_hcu_setkey,
	.base.halg = {
		.digestsize	= SHA384_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "hmac(sha384)",
			.cra_driver_name	= "hmac-sha384-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA384_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_hmac_cra_init,
			.cra_exit		= kmb_ocs_hcu_hmac_cra_exit,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.halg = {
		.digestsize	= SHA512_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "sha512",
			.cra_driver_name	= "sha512-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA512_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_sha_cra_init,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
{
	.base.init		= kmb_ocs_hcu_init,
	.base.update		= kmb_ocs_hcu_update,
	.base.final		= kmb_ocs_hcu_final,
	.base.finup		= kmb_ocs_hcu_finup,
	.base.digest		= kmb_ocs_hcu_digest,
	.base.export		= kmb_ocs_hcu_export,
	.base.import		= kmb_ocs_hcu_import,
	.base.setkey		= kmb_ocs_hcu_setkey,
	.base.halg = {
		.digestsize	= SHA512_DIGEST_SIZE,
		.statesize	= sizeof(struct ocs_hcu_rctx),
		.base	= {
			.cra_name		= "hmac(sha512)",
			.cra_driver_name	= "hmac-sha512-keembay-ocs",
			.cra_priority		= 255,
			.cra_flags		= CRYPTO_ALG_ASYNC,
			.cra_blocksize		= SHA512_BLOCK_SIZE,
			.cra_ctxsize		= sizeof(struct ocs_hcu_ctx),
			.cra_alignmask		= 0,
			.cra_module		= THIS_MODULE,
			.cra_init		= kmb_ocs_hcu_hmac_cra_init,
			.cra_exit		= kmb_ocs_hcu_hmac_cra_exit,
		}
	},
	.op.do_one_request = kmb_ocs_hcu_do_one_request,
},
};

 
static const struct of_device_id kmb_ocs_hcu_of_match[] = {
	{
		.compatible = "intel,keembay-ocs-hcu",
	},
	{}
};

static int kmb_ocs_hcu_remove(struct platform_device *pdev)
{
	struct ocs_hcu_dev *hcu_dev;
	int rc;

	hcu_dev = platform_get_drvdata(pdev);
	if (!hcu_dev)
		return -ENODEV;

	crypto_engine_unregister_ahashes(ocs_hcu_algs, ARRAY_SIZE(ocs_hcu_algs));

	rc = crypto_engine_exit(hcu_dev->engine);

	spin_lock_bh(&ocs_hcu.lock);
	list_del(&hcu_dev->list);
	spin_unlock_bh(&ocs_hcu.lock);

	return rc;
}

static int kmb_ocs_hcu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ocs_hcu_dev *hcu_dev;
	int rc;

	hcu_dev = devm_kzalloc(dev, sizeof(*hcu_dev), GFP_KERNEL);
	if (!hcu_dev)
		return -ENOMEM;

	hcu_dev->dev = dev;

	platform_set_drvdata(pdev, hcu_dev);
	rc = dma_set_mask_and_coherent(&pdev->dev, OCS_HCU_DMA_BIT_MASK);
	if (rc)
		return rc;

	hcu_dev->io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(hcu_dev->io_base))
		return PTR_ERR(hcu_dev->io_base);

	init_completion(&hcu_dev->irq_done);

	 
	hcu_dev->irq = platform_get_irq(pdev, 0);
	if (hcu_dev->irq < 0)
		return hcu_dev->irq;

	rc = devm_request_threaded_irq(&pdev->dev, hcu_dev->irq,
				       ocs_hcu_irq_handler, NULL, 0,
				       "keembay-ocs-hcu", hcu_dev);
	if (rc < 0) {
		dev_err(dev, "Could not request IRQ.\n");
		return rc;
	}

	INIT_LIST_HEAD(&hcu_dev->list);

	spin_lock_bh(&ocs_hcu.lock);
	list_add_tail(&hcu_dev->list, &ocs_hcu.dev_list);
	spin_unlock_bh(&ocs_hcu.lock);

	 
	hcu_dev->engine = crypto_engine_alloc_init(dev, 1);
	if (!hcu_dev->engine) {
		rc = -ENOMEM;
		goto list_del;
	}

	rc = crypto_engine_start(hcu_dev->engine);
	if (rc) {
		dev_err(dev, "Could not start engine.\n");
		goto cleanup;
	}

	 

	rc = crypto_engine_register_ahashes(ocs_hcu_algs, ARRAY_SIZE(ocs_hcu_algs));
	if (rc) {
		dev_err(dev, "Could not register algorithms.\n");
		goto cleanup;
	}

	return 0;

cleanup:
	crypto_engine_exit(hcu_dev->engine);
list_del:
	spin_lock_bh(&ocs_hcu.lock);
	list_del(&hcu_dev->list);
	spin_unlock_bh(&ocs_hcu.lock);

	return rc;
}

 
static struct platform_driver kmb_ocs_hcu_driver = {
	.probe = kmb_ocs_hcu_probe,
	.remove = kmb_ocs_hcu_remove,
	.driver = {
			.name = DRV_NAME,
			.of_match_table = kmb_ocs_hcu_of_match,
		},
};

module_platform_driver(kmb_ocs_hcu_driver);

MODULE_LICENSE("GPL");
